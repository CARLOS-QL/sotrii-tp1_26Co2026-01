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

/* Demo includes */
#include "logger.h"
#include "dwt.h"

/* Application & Tasks includes */
#include "board.h"
#include "app.h"
#include "app_it.h"
#include "task_adc.h"
#include "task_adc_attribute.h"
#include "task_adc_interface.h"

/********************** macros and definitions *******************************/

/********************** internal data declaration ****************************/
task_adc_dta_t task_adc_dta;

/********************** internal functions declaration ***********************/
static void adc_if_update_wcet(uint32_t elapsed_us, uint32_t *p_wcet_us);
static HAL_StatusTypeDef adc_enqueue_cmd(task_adc_in_cmd_t cmd, TickType_t timeout_ticks);

/********************** internal data definition *****************************/

/********************** external data declaration ****************************/
uint32_t g_adc_if_open_wcet_us;
uint32_t g_adc_if_release_wcet_us;
uint32_t g_adc_if_write_wcet_us;
uint32_t g_adc_if_read_wcet_us;
uint32_t g_adc_if_ioctl_wcet_us;
volatile uint32_t g_adc_dma_isr_cnt;

/********************** external functions definition ************************/
static void adc_if_update_wcet(uint32_t elapsed_us, uint32_t *p_wcet_us)
{
	if (elapsed_us > *p_wcet_us)
	{
		*p_wcet_us = elapsed_us;
	}
}

static HAL_StatusTypeDef adc_enqueue_cmd(task_adc_in_cmd_t cmd, TickType_t timeout_ticks)
{
	HAL_StatusTypeDef hal_status = HAL_ERROR;
	task_adc_dta_t *p_task_adc_dta = &task_adc_dta;
	task_adc_in_dta_t in_dta;

	in_dta.cmd = cmd;

	if ((NULL != p_task_adc_dta->device_id) &&
		(NULL != p_task_adc_dta->queue_in))
	{
		if (pdPASS == xQueueSend(p_task_adc_dta->queue_in, &in_dta, timeout_ticks))
		{
			hal_status = HAL_OK;
		}
		else
		{
			hal_status = HAL_BUSY;
		}
	}

	return hal_status;
}

/*
 * Paso 06 - Device Driver ADC FreeRTOS
 * ------------------------------------
 * Patron: Gatekeeper (task_adc) + Latest Input Only (LIO).
 * Periferico: HAL_ADC_Start_DMA (ADC1 / PA0 - potenciometro).
 * Almacenamiento: colas Input/Output Spooler estaticas.
 */

void open_adc(ADC_HandleTypeDef *h_adc_device)
{
	BaseType_t ret;
	uint32_t elapsed_us;
	task_adc_dta_t *p_task_adc_dta = &task_adc_dta;

	cycle_counter_reset();

	p_task_adc_dta->device_id = h_adc_device;
	p_task_adc_dta->adc_channel = ADC_CHANNEL_0;
	p_task_adc_dta->sampling_active = pdFALSE;
	p_task_adc_dta->dma_buffer[0] = 0u;

	p_task_adc_dta->queue_in = xQueueCreateStatic(TASK_ADC_QUEUE_IN_LEN,
												  sizeof(task_adc_in_dta_t),
												  p_task_adc_dta->queue_in_storage,
												  &p_task_adc_dta->queue_in_struct);
	configASSERT(NULL != p_task_adc_dta->queue_in);
	vQueueAddToRegistry(p_task_adc_dta->queue_in, "Task ADC In Spooler");

	p_task_adc_dta->queue_out = xQueueCreateStatic(TASK_ADC_QUEUE_OUT_LEN,
												   sizeof(uint16_t),
												   p_task_adc_dta->queue_out_storage,
												   &p_task_adc_dta->queue_out_struct);
	configASSERT(NULL != p_task_adc_dta->queue_out);
	vQueueAddToRegistry(p_task_adc_dta->queue_out, "Task ADC Out Spooler");

	p_task_adc_dta->sem_dma_done = xSemaphoreCreateBinaryStatic(
		&p_task_adc_dta->sem_dma_done_struct);
	configASSERT(NULL != p_task_adc_dta->sem_dma_done);

	ret = xTaskCreate(task_adc, "Task ADC", (2u * configMINIMAL_STACK_SIZE),
					  (void *)p_task_adc_dta,
					  (tskIDLE_PRIORITY + 2ul), &p_task_adc_dta->task_adc);
	configASSERT(pdPASS == ret);

	elapsed_us = cycle_counter_get_time_us();
	adc_if_update_wcet(elapsed_us, &g_adc_if_open_wcet_us);
}

void release_adc(ADC_HandleTypeDef *h_adc_device)
{
	uint32_t elapsed_us;
	task_adc_dta_t *p_task_adc_dta = &task_adc_dta;
	task_adc_in_dta_t pending_cmd;
	uint16_t pending_value;

	cycle_counter_reset();

	if (p_task_adc_dta->device_id == h_adc_device)
	{
		p_task_adc_dta->sampling_active = pdFALSE;
		(void)HAL_ADC_Stop_DMA(p_task_adc_dta->device_id);

		vTaskDelete(p_task_adc_dta->task_adc);

		while (pdPASS == xQueueReceive(p_task_adc_dta->queue_in, &pending_cmd, 0))
		{
			/* descartar comandos pendientes */
		}

		while (pdPASS == xQueueReceive(p_task_adc_dta->queue_out, &pending_value, 0))
		{
			/* vaciar output spooler LIO */
		}

		vQueueUnregisterQueue(p_task_adc_dta->queue_in);
		vQueueDelete(p_task_adc_dta->queue_in);
		vQueueUnregisterQueue(p_task_adc_dta->queue_out);
		vQueueDelete(p_task_adc_dta->queue_out);

		vSemaphoreDelete(p_task_adc_dta->sem_dma_done);

		p_task_adc_dta->device_id = NULL;
		p_task_adc_dta->task_adc = NULL;
		p_task_adc_dta->queue_in = NULL;
		p_task_adc_dta->queue_out = NULL;
		p_task_adc_dta->sem_dma_done = NULL;
	}

	elapsed_us = cycle_counter_get_time_us();
	adc_if_update_wcet(elapsed_us, &g_adc_if_release_wcet_us);
}

HAL_StatusTypeDef write_adc(ADC_HandleTypeDef *h_adc_device)
{
	HAL_StatusTypeDef hal_status = HAL_ERROR;
	uint32_t elapsed_us;
	task_adc_dta_t *p_task_adc_dta = &task_adc_dta;

	cycle_counter_reset();

	if ((NULL != p_task_adc_dta->device_id) &&
		(p_task_adc_dta->device_id == h_adc_device))
	{
		/* Asincronico: solicitar una conversion al gatekeeper */
		hal_status = adc_enqueue_cmd(ADC_IN_CMD_SAMPLE, pdMS_TO_TICKS(100u));
	}

	elapsed_us = cycle_counter_get_time_us();
	adc_if_update_wcet(elapsed_us, &g_adc_if_write_wcet_us);

	return hal_status;
}

BaseType_t read_adc(ADC_HandleTypeDef *h_adc_device, uint16_t *p_value)
{
	BaseType_t ret = pdFALSE;
	uint32_t elapsed_us;
	task_adc_dta_t *p_task_adc_dta = &task_adc_dta;
	uint16_t adc_value = 0u;

	cycle_counter_reset();

	if ((NULL != p_task_adc_dta->device_id) &&
		(p_task_adc_dta->device_id == h_adc_device) &&
		(NULL != p_value) &&
		(NULL != p_task_adc_dta->queue_out))
	{
		/* LIO: leer la ultima muestra disponible sin bloquear */
		if (pdPASS == xQueueReceive(p_task_adc_dta->queue_out, &adc_value, 0))
		{
			*p_value = adc_value;
			ret = pdTRUE;
		}
	}

	elapsed_us = cycle_counter_get_time_us();
	adc_if_update_wcet(elapsed_us, &g_adc_if_read_wcet_us);

	return ret;
}

BaseType_t read_adc_wait(ADC_HandleTypeDef *h_adc_device,
						  uint16_t *p_value,
						  TickType_t timeout_ticks)
{
	BaseType_t ret = pdFALSE;
	task_adc_dta_t *p_task_adc_dta = &task_adc_dta;
	uint16_t adc_value = 0u;

	if ((NULL != p_task_adc_dta->device_id) &&
		(p_task_adc_dta->device_id == h_adc_device) &&
		(NULL != p_value) &&
		(NULL != p_task_adc_dta->queue_out))
	{
		if (pdPASS == xQueueReceive(p_task_adc_dta->queue_out, &adc_value, timeout_ticks))
		{
			*p_value = adc_value;
			ret = pdTRUE;
		}
	}

	return ret;
}

HAL_StatusTypeDef ioctl_adc(ADC_HandleTypeDef *h_adc_device,
							  task_adc_ioctl_cmd_t cmd)
{
	HAL_StatusTypeDef hal_status = HAL_ERROR;
	uint32_t elapsed_us;
	task_adc_dta_t *p_task_adc_dta = &task_adc_dta;
	uint16_t dummy_value;

	cycle_counter_reset();

	if (p_task_adc_dta->device_id == h_adc_device)
	{
		switch (cmd)
		{
			case ADC_IOCTL_FLUSH:
				while (pdPASS == xQueueReceive(p_task_adc_dta->queue_out,
											   &dummy_value, 0))
				{
					/* vaciar output spooler LIO */
				}
				hal_status = HAL_OK;
				break;

			case ADC_IOCTL_START_SAMPLING:
				hal_status = adc_enqueue_cmd(ADC_IN_CMD_START, pdMS_TO_TICKS(100u));
				break;

			case ADC_IOCTL_STOP_SAMPLING:
				hal_status = adc_enqueue_cmd(ADC_IN_CMD_STOP, pdMS_TO_TICKS(100u));
				break;

			default:
				hal_status = HAL_ERROR;
				break;
		}
	}

	elapsed_us = cycle_counter_get_time_us();
	adc_if_update_wcet(elapsed_us, &g_adc_if_ioctl_wcet_us);

	return hal_status;
}

void adc_if_wcet_report(void)
{
	extern uint32_t g_task_adc_runtime_us;

	LOGGER_INFO(" ");
	LOGGER_INFO("=== WCET Funciones Interfaz ADC [us] ===");
	LOGGER_INFO("  open_adc()    : %lu", g_adc_if_open_wcet_us);
	LOGGER_INFO("  release_adc() : %lu", g_adc_if_release_wcet_us);
	LOGGER_INFO("  write_adc()   : %lu", g_adc_if_write_wcet_us);
	LOGGER_INFO("  read_adc()    : %lu", g_adc_if_read_wcet_us);
	LOGGER_INFO("  ioctl_adc()   : %lu", g_adc_if_ioctl_wcet_us);
	LOGGER_INFO("=== WCET Gatekeeper ADC [us] ===");
	LOGGER_INFO("  task_adc      : %lu", g_task_adc_runtime_us);
	LOGGER_INFO("  DMA ISR cnt   : %lu", g_adc_dma_isr_cnt);
}

void adc_dma_cplt_notify_from_isr(BaseType_t *p_higher_priority_woken)
{
	task_adc_dta_t *p_task_adc_dta = &task_adc_dta;

	g_adc_dma_isr_cnt++;

	if (NULL != p_task_adc_dta->sem_dma_done)
	{
		(void)xSemaphoreGiveFromISR(p_task_adc_dta->sem_dma_done,
									p_higher_priority_woken);
	}
}

/********************** end of file ******************************************/
