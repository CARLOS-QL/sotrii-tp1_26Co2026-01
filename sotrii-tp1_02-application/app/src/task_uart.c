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
#include "task_uart_attribute.h"
#include "task_uart_interface.h"

/********************** macros and definitions *******************************/
#define G_TASK_UART_CNT_INI			0ul
#define G_TASK_UART_RUNTIME_US_INI	0ul

/********************** internal data declaration ****************************/

/********************** internal functions declaration ***********************/

/********************** internal data definition *****************************/

/********************** external data declaration ****************************/
uint32_t g_task_uart_tx_cnt;
uint32_t g_task_uart_tx_runtime_us;

uint32_t g_task_uart_rx_cnt;
uint32_t g_task_uart_rx_runtime_us;

/********************** external functions definition ************************/
/*
 * Gatekeeper TX: unica tarea autorizada a transmitir por USART2.
 * Lee del input spooler, transfiere con HAL_UART_Transmit_IT y
 * publica el estado en el output spooler (patron Asynchronous).
 */
void task_uart_tx(void *parameters)
{
	HAL_StatusTypeDef hal_status;
	task_uart_dta_t *p_task_uart_dta = (task_uart_dta_t *)parameters;
	task_uart_tx_out_dta_t tx_out_dta;

	g_task_uart_tx_cnt = G_TASK_UART_CNT_INI;
	g_task_uart_tx_runtime_us = G_TASK_UART_RUNTIME_US_INI;

	LOGGER_INFO(" ");
	LOGGER_INFO("%s is running - Tick [mS] = %3d", pcTaskGetName(NULL), (int)xTaskGetTickCount());

	for (;;)
	{
		g_task_uart_tx_cnt++;

		xQueueReceive(p_task_uart_dta->queue_tx_in,
					  &p_task_uart_dta->tx_active, portMAX_DELAY);

		cycle_counter_reset();

		while (pdTRUE == xSemaphoreTake(p_task_uart_dta->sem_tx_done, 0))
		{
			/* descartar eventos previos del semaforo TX */
		}

		hal_status = HAL_UART_Transmit_IT(p_task_uart_dta->device_id,
										  p_task_uart_dta->tx_active.p_buffer,
										  p_task_uart_dta->tx_active.length);

		if (HAL_OK == hal_status)
		{
			(void)xSemaphoreTake(p_task_uart_dta->sem_tx_done, portMAX_DELAY);
			tx_out_dta.status = UART_SPOOLER_STATUS_OK;
			tx_out_dta.bytes_done = p_task_uart_dta->tx_active.length;
		}
		else
		{
			tx_out_dta.status = UART_SPOOLER_STATUS_ERROR;
			tx_out_dta.bytes_done = 0u;
		}

		(void)xQueueSend(p_task_uart_dta->queue_tx_out, &tx_out_dta, 0);

		if (NULL != p_task_uart_dta->tx_active.p_buffer)
		{
			vPortFree(p_task_uart_dta->tx_active.p_buffer);
			p_task_uart_dta->tx_active.p_buffer = NULL;
			p_task_uart_dta->tx_active.length = 0u;
		}

		g_task_uart_tx_runtime_us = cycle_counter_get_time_us();
	}
}

/*
 * Gatekeeper RX: arma la recepcion por interrupcion y atiende
 * comandos del input spooler (flush / re-arm).
 */
void task_uart_rx(void *parameters)
{
	task_uart_dta_t *p_task_uart_dta = (task_uart_dta_t *)parameters;
	task_uart_rx_in_dta_t rx_cmd;
	uint8_t rx_dummy;

	g_task_uart_rx_cnt = G_TASK_UART_CNT_INI;
	g_task_uart_rx_runtime_us = G_TASK_UART_RUNTIME_US_INI;

	LOGGER_INFO(" ");
	LOGGER_INFO("%s is running - Tick [mS] = %3d", pcTaskGetName(NULL), (int)xTaskGetTickCount());

	(void)ioctl_uart(p_task_uart_dta->device_id, UART_IOCTL_ARM_RX);

	for (;;)
	{
		g_task_uart_rx_cnt++;

		xQueueReceive(p_task_uart_dta->queue_rx_in, &rx_cmd, portMAX_DELAY);

		cycle_counter_reset();

		switch (rx_cmd.cmd)
		{
			case UART_RX_CMD_ARM:
				(void)ioctl_uart(p_task_uart_dta->device_id, UART_IOCTL_ARM_RX);
				break;

			case UART_RX_CMD_FLUSH:
				while (pdPASS == xQueueReceive(p_task_uart_dta->queue_rx_out,
											   &rx_dummy, 0))
				{
					/* vaciar output spooler RX */
				}
				break;

			default:
				break;
		}

		g_task_uart_rx_runtime_us = cycle_counter_get_time_us();
	}
}

/********************** end of file ******************************************/
