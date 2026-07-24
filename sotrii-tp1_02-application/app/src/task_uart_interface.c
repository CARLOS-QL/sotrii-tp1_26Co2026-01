/*
 * Copyright (c) 2026 Juan Manuel Cruz <jcruz@fi.uba.ar> <jcruz@frba.utn.edu.ar>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * @author : Juan Manuel Cruz <jcruz@fi.uba.ar> <jcruz@frba.utn.edu.ar>
 */

/********************** inclusions *******************************************/
/* Project includes */
#include "main.h"
#include "cmsis_os.h"
#include <string.h>

/* Demo includes */
#include "logger.h"
#include "dwt.h"

/* Application & Tasks includes */
#include "board.h"
#include "app.h"
#include "app_it.h"
#include "task_uart.h"
#include "task_uart_attribute.h"
#include "task_uart_interface.h"

/********************** macros and definitions *******************************/

/********************** internal data declaration ****************************/
task_uart_dta_t task_uart_dta;

/********************** internal functions declaration ***********************/
static void uart_if_update_wcet(uint32_t elapsed_us, uint32_t *p_wcet_us);
static void uart_free_spooler(task_uart_spooler_dta_t *p_spooler);
static HAL_StatusTypeDef uart_arm_rx_channel(UART_HandleTypeDef *h_uart_device);

/********************** internal data definition *****************************/

/********************** external data declaration ****************************/
uint32_t g_uart_if_open_wcet_us;
uint32_t g_uart_if_release_wcet_us;
uint32_t g_uart_if_write_wcet_us;
uint32_t g_uart_if_read_wcet_us;
uint32_t g_uart_if_ioctl_wcet_us;
volatile uint32_t g_uart_rx_isr_cnt;

/********************** external functions definition ************************/
static void uart_if_update_wcet(uint32_t elapsed_us, uint32_t *p_wcet_us)
{
	if (elapsed_us > *p_wcet_us)
	{
		*p_wcet_us = elapsed_us;
	}
}

static void uart_free_spooler(task_uart_spooler_dta_t *p_spooler)
{
	if ((NULL != p_spooler) && (NULL != p_spooler->p_buffer))
	{
		vPortFree(p_spooler->p_buffer);
		p_spooler->p_buffer = NULL;
		p_spooler->length = 0u;
	}
}

static HAL_StatusTypeDef uart_arm_rx_channel(UART_HandleTypeDef *h_uart_device)
{
	task_uart_dta_t *p_task_uart_dta = &task_uart_dta;

	if (p_task_uart_dta->device_id != h_uart_device)
	{
		return HAL_ERROR;
	}

	/* Ya hay una recepcion IT activa: no abortar */
	if (HAL_UART_STATE_BUSY_RX == p_task_uart_dta->device_id->RxState)
	{
		p_task_uart_dta->rx_armed = pdTRUE;
		return HAL_OK;
	}

	if (HAL_UART_STATE_READY != p_task_uart_dta->device_id->RxState)
	{
		(void)HAL_UART_AbortReceive_IT(p_task_uart_dta->device_id);
	}

	if (HAL_OK == HAL_UART_Receive_IT(p_task_uart_dta->device_id,
									  &p_task_uart_dta->rx_byte, 1u))
	{
	 p_task_uart_dta->rx_armed = pdTRUE;
		return HAL_OK;
	}

	p_task_uart_dta->rx_armed = pdFALSE;
	return HAL_ERROR;
}

/*
 * Paso 06 - Device Driver UART FreeRTOS
 * -------------------------------------
 * Patron: Gatekeeper (task_uart_tx / task_uart_rx) + Asynchronous API.
 * Periferico: HAL_UART_Transmit_IT / HAL_UART_Receive_IT (interrupciones).
 * Almacenamiento: colas Input/Output Spooler + pvPortMalloc.
 */

void open_uart(UART_HandleTypeDef *h_uart_device)
{
	BaseType_t ret;
	uint32_t elapsed_us;
	task_uart_dta_t *p_task_uart_dta = &task_uart_dta;
	task_uart_rx_in_dta_t rx_arm_cmd;

	cycle_counter_reset();

	p_task_uart_dta->device_id = h_uart_device;
	p_task_uart_dta->tx_active.p_buffer = NULL;
	p_task_uart_dta->tx_active.length = 0u;
	p_task_uart_dta->rx_armed = pdFALSE;

	p_task_uart_dta->queue_tx_in = xQueueCreate(TASK_UART_QUEUE_TX_IN_LEN,
												sizeof(task_uart_spooler_dta_t));
	configASSERT(NULL != p_task_uart_dta->queue_tx_in);
	vQueueAddToRegistry(p_task_uart_dta->queue_tx_in, "Task UART Tx In Spooler");

	p_task_uart_dta->queue_tx_out = xQueueCreate(TASK_UART_QUEUE_TX_OUT_LEN,
												 sizeof(task_uart_tx_out_dta_t));
	configASSERT(NULL != p_task_uart_dta->queue_tx_out);
	vQueueAddToRegistry(p_task_uart_dta->queue_tx_out, "Task UART Tx Out Spooler");

	p_task_uart_dta->queue_rx_in = xQueueCreate(TASK_UART_QUEUE_RX_IN_LEN,
												sizeof(task_uart_rx_in_dta_t));
	configASSERT(NULL != p_task_uart_dta->queue_rx_in);
	vQueueAddToRegistry(p_task_uart_dta->queue_rx_in, "Task UART Rx In Spooler");

	p_task_uart_dta->queue_rx_out = xQueueCreate(TASK_UART_QUEUE_RX_OUT_LEN,
												 sizeof(uint8_t));
	configASSERT(NULL != p_task_uart_dta->queue_rx_out);
	vQueueAddToRegistry(p_task_uart_dta->queue_rx_out, "Task UART Rx Out Spooler");

	p_task_uart_dta->sem_tx_done = xSemaphoreCreateBinary();
	configASSERT(NULL != p_task_uart_dta->sem_tx_done);

	ret = xTaskCreate(task_uart_tx, "Task UART Tx", (2u * configMINIMAL_STACK_SIZE),
					  (void *)p_task_uart_dta,
					  (tskIDLE_PRIORITY + 1ul), &p_task_uart_dta->task_tx);
	configASSERT(pdPASS == ret);

	ret = xTaskCreate(task_uart_rx, "Task UART Rx", (2u * configMINIMAL_STACK_SIZE),
					  (void *)p_task_uart_dta,
					  (tskIDLE_PRIORITY + 1ul), &p_task_uart_dta->task_rx);
	configASSERT(pdPASS == ret);

	rx_arm_cmd.cmd = UART_RX_CMD_ARM;
	(void)xQueueSend(p_task_uart_dta->queue_rx_in, &rx_arm_cmd, 0);

	elapsed_us = cycle_counter_get_time_us();
	uart_if_update_wcet(elapsed_us, &g_uart_if_open_wcet_us);
}

void release_uart(UART_HandleTypeDef *h_uart_device)
{
	uint32_t elapsed_us;
	task_uart_dta_t *p_task_uart_dta = &task_uart_dta;
	task_uart_spooler_dta_t pending_spooler;

	cycle_counter_reset();

	if (p_task_uart_dta->device_id == h_uart_device)
	{
		vTaskDelete(p_task_uart_dta->task_tx);
		vTaskDelete(p_task_uart_dta->task_rx);

		while (pdPASS == xQueueReceive(p_task_uart_dta->queue_tx_in,
									   &pending_spooler, 0))
		{
			uart_free_spooler(&pending_spooler);
		}

		uart_free_spooler(&p_task_uart_dta->tx_active);

		vQueueUnregisterQueue(p_task_uart_dta->queue_tx_in);
		vQueueDelete(p_task_uart_dta->queue_tx_in);
		vQueueUnregisterQueue(p_task_uart_dta->queue_tx_out);
		vQueueDelete(p_task_uart_dta->queue_tx_out);
		vQueueUnregisterQueue(p_task_uart_dta->queue_rx_in);
		vQueueDelete(p_task_uart_dta->queue_rx_in);
		vQueueUnregisterQueue(p_task_uart_dta->queue_rx_out);
		vQueueDelete(p_task_uart_dta->queue_rx_out);

		vSemaphoreDelete(p_task_uart_dta->sem_tx_done);

		p_task_uart_dta->device_id = NULL;
		p_task_uart_dta->task_tx = NULL;
		p_task_uart_dta->task_rx = NULL;
		p_task_uart_dta->queue_tx_in = NULL;
		p_task_uart_dta->queue_tx_out = NULL;
		p_task_uart_dta->queue_rx_in = NULL;
		p_task_uart_dta->queue_rx_out = NULL;
		p_task_uart_dta->sem_tx_done = NULL;
		p_task_uart_dta->rx_armed = pdFALSE;
	}

	elapsed_us = cycle_counter_get_time_us();
	uart_if_update_wcet(elapsed_us, &g_uart_if_release_wcet_us);
}

HAL_StatusTypeDef write_uart(UART_HandleTypeDef *h_uart_device,
							 const uint8_t *p_data,
							 uint16_t length)
{
	HAL_StatusTypeDef hal_status = HAL_ERROR;
	uint32_t elapsed_us;
	task_uart_dta_t *p_task_uart_dta = &task_uart_dta;
	task_uart_spooler_dta_t spooler;

	cycle_counter_reset();

	if ((NULL != p_task_uart_dta->device_id) &&
		(p_task_uart_dta->device_id == h_uart_device) &&
		(NULL != p_data) && (0u < length))
	{
		spooler.p_buffer = (uint8_t *)pvPortMalloc(length);
		if (NULL != spooler.p_buffer)
		{
			(void)memcpy(spooler.p_buffer, p_data, length);
			spooler.length = length;

			/* Asynchronous: encolar en input spooler TX (espera breve si esta ocupado) */
			if (pdPASS == xQueueSend(p_task_uart_dta->queue_tx_in, &spooler,
									 pdMS_TO_TICKS(500u)))
			{
				hal_status = HAL_OK;
			}
			else
			{
				uart_free_spooler(&spooler);
				hal_status = HAL_BUSY;
			}
		}
	}

	elapsed_us = cycle_counter_get_time_us();
	uart_if_update_wcet(elapsed_us, &g_uart_if_write_wcet_us);

	return hal_status;
}

BaseType_t read_uart(UART_HandleTypeDef *h_uart_device, uint8_t *p_data)
{
	BaseType_t ret = pdFALSE;
	uint32_t elapsed_us;
	task_uart_dta_t *p_task_uart_dta = &task_uart_dta;
	uint8_t rx_byte = 0u;

	cycle_counter_reset();

	if ((NULL != p_task_uart_dta->device_id) &&
		(p_task_uart_dta->device_id == h_uart_device) &&
		(NULL != p_data))
	{
		/* Asynchronous: leer del output spooler RX sin bloquear */
		if (pdPASS == xQueueReceive(p_task_uart_dta->queue_rx_out, &rx_byte, 0))
		{
			*p_data = rx_byte;
			ret = pdTRUE;
		}
	}

	elapsed_us = cycle_counter_get_time_us();
	uart_if_update_wcet(elapsed_us, &g_uart_if_read_wcet_us);

	return ret;
}

BaseType_t read_uart_wait(UART_HandleTypeDef *h_uart_device,
						  uint8_t *p_data,
						  TickType_t timeout_ticks)
{
	BaseType_t ret = pdFALSE;
	task_uart_dta_t *p_task_uart_dta = &task_uart_dta;
	uint8_t rx_byte = 0u;

	if ((NULL != p_task_uart_dta->device_id) &&
		(p_task_uart_dta->device_id == h_uart_device) &&
		(NULL != p_data) &&
		(NULL != p_task_uart_dta->queue_rx_out))
	{
		if (pdPASS == xQueueReceive(p_task_uart_dta->queue_rx_out, &rx_byte, timeout_ticks))
		{
			*p_data = rx_byte;
			ret = pdTRUE;
		}
	}

	return ret;
}

HAL_StatusTypeDef ioctl_uart(UART_HandleTypeDef *h_uart_device,
							 task_uart_ioctl_cmd_t cmd)
{
	HAL_StatusTypeDef hal_status = HAL_ERROR;
	uint32_t elapsed_us;
	task_uart_dta_t *p_task_uart_dta = &task_uart_dta;
	task_uart_rx_in_dta_t rx_cmd;
	task_uart_spooler_dta_t pending_spooler;
	task_uart_tx_out_dta_t tx_out_dummy;

	cycle_counter_reset();

	if (p_task_uart_dta->device_id == h_uart_device)
	{
		switch (cmd)
		{
			case UART_IOCTL_FLUSH_RX:
				rx_cmd.cmd = UART_RX_CMD_FLUSH;
				if (pdPASS == xQueueSend(p_task_uart_dta->queue_rx_in, &rx_cmd, portMAX_DELAY))
				{
					hal_status = HAL_OK;
				}
				break;

			case UART_IOCTL_FLUSH_TX:
				while (pdPASS == xQueueReceive(p_task_uart_dta->queue_tx_in,
											   &pending_spooler, 0))
				{
					uart_free_spooler(&pending_spooler);
				}
				while (pdPASS == xQueueReceive(p_task_uart_dta->queue_tx_out,
											   &tx_out_dummy, 0))
				{
					/* descartar estados pendientes */
				}
				hal_status = HAL_OK;
				break;

			case UART_IOCTL_ABORT_TX:
				hal_status = HAL_UART_AbortTransmit_IT(h_uart_device);
				uart_free_spooler(&p_task_uart_dta->tx_active);
				break;

			case UART_IOCTL_ARM_RX:
				hal_status = uart_arm_rx_channel(h_uart_device);
				break;

			default:
				hal_status = HAL_ERROR;
				break;
		}
	}

	elapsed_us = cycle_counter_get_time_us();
	uart_if_update_wcet(elapsed_us, &g_uart_if_ioctl_wcet_us);

	return hal_status;
}

void uart_if_wcet_report(void)
{
	extern uint32_t g_task_uart_tx_runtime_us;
	extern uint32_t g_task_uart_rx_runtime_us;

	LOGGER_INFO(" ");
	LOGGER_INFO("=== WCET Funciones Interfaz UART [us] ===");
	LOGGER_INFO("  open_uart()    : %lu", g_uart_if_open_wcet_us);
	LOGGER_INFO("  release_uart() : %lu", g_uart_if_release_wcet_us);
	LOGGER_INFO("  write_uart()   : %lu", g_uart_if_write_wcet_us);
	LOGGER_INFO("  read_uart()    : %lu", g_uart_if_read_wcet_us);
	LOGGER_INFO("  ioctl_uart()   : %lu", g_uart_if_ioctl_wcet_us);
	LOGGER_INFO("=== WCET Gatekeeper UART [us] ===");
	LOGGER_INFO("  task_uart_tx   : %lu", g_task_uart_tx_runtime_us);
	LOGGER_INFO("  task_uart_rx   : %lu", g_task_uart_rx_runtime_us);
}

void uart_tx_cplt_notify_from_isr(BaseType_t *p_higher_priority_woken)
{
	task_uart_dta_t *p_task_uart_dta = &task_uart_dta;

	if (NULL != p_task_uart_dta->sem_tx_done)
	{
		(void)xSemaphoreGiveFromISR(p_task_uart_dta->sem_tx_done,
									p_higher_priority_woken);
	}
}

void uart_rx_cplt_notify_from_isr(uint8_t rx_byte,
								  BaseType_t *p_higher_priority_woken)
{
	task_uart_dta_t *p_task_uart_dta = &task_uart_dta;

	g_uart_rx_isr_cnt++;

	if (NULL != p_task_uart_dta->queue_rx_out)
	{
		if (pdPASS != xQueueSendFromISR(p_task_uart_dta->queue_rx_out, &rx_byte,
										p_higher_priority_woken))
		{
			/* output spooler RX lleno: descartar byte */
		}
	}
}

void uart_error_notify_from_isr(BaseType_t *p_higher_priority_woken)
{
	task_uart_dta_t *p_task_uart_dta = &task_uart_dta;
	task_uart_rx_in_dta_t rx_cmd;

	if (NULL != p_task_uart_dta->queue_rx_in)
	{
		rx_cmd.cmd = UART_RX_CMD_ARM;
		(void)xQueueSendFromISR(p_task_uart_dta->queue_rx_in, &rx_cmd,
								p_higher_priority_woken);
	}
}

/********************** end of file ******************************************/
