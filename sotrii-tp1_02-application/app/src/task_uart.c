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

/********************** macros and definitions *******************************/
#define G_TASK_XXXX_CNT_INI	0ul
#define G_TASK_XXXX_RUNTIME_US_INI	0ul

#define TASK_XXXX_DEL_ZERO	(pdMS_TO_TICKS(0ul))
#define TASK_XXXX_DEL_MAX	(pdMS_TO_TICKS(250ul))

/********************** internal data declaration ****************************/

/********************** internal data declaration ****************************/

/********************** internal functions declaration ***********************/
void task_uart_tx(void *parameters);
void task_uart_rx(void *parameters);

/********************** internal data definition *****************************/
const char *p_task_uart_tx_wait_250mS	= "   ==> Task UART TX - Wait:   250mS";
const char *p_task_uart_rx_wait_250mS	= "   ==> Task UART RX - Wait:   250mS";

/********************** external data declaration ****************************/
uint32_t g_task_xxxx_tx_cnt;
uint32_t g_task_xxxx_tx_runtime_us;

uint32_t g_task_xxxx_rx_cnt;
uint32_t g_task_xxxx_rx_runtime_us;

/********************** external functions definition ************************/
/* Task UART TX thread */
void task_uart_tx(void *parameters)
{
	/* Prevent unused argument(s) compilation warning */
	UNUSED(parameters);

	/*  Declare & Initialize Task Function variables */
	g_task_xxxx_tx_cnt = G_TASK_XXXX_CNT_INI;
	g_task_xxxx_tx_runtime_us = G_TASK_XXXX_RUNTIME_US_INI;

	/* Print out: Task Initialized */
	LOGGER_INFO(" ");
	LOGGER_INFO("%s is running - Tick [mS] = %3d", pcTaskGetName(NULL), (int)xTaskGetTickCount());

	/* As per most tasks, this task is implemented in an infinite loop. */
	for (;;)
	{
		/* Update Task Counter */
		g_task_xxxx_tx_cnt++;

		cycle_counter_reset();

		HAL_GPIO_TogglePin(LED_A_PORT, LED_A_PIN);

		g_task_xxxx_tx_runtime_us = cycle_counter_get_time_us();

    	/* Print out: Wait 250mS */
		LOGGER_INFO(p_task_uart_tx_wait_250mS);
		vTaskDelay(TASK_XXXX_DEL_MAX);
	}
}

/* Task UART RX thread */
void task_uart_rx(void *parameters)
{
	/* Prevent unused argument(s) compilation warning */
	UNUSED(parameters);

	/*  Declare & Initialize Task Function variables */
	g_task_xxxx_rx_cnt = G_TASK_XXXX_CNT_INI;
	g_task_xxxx_rx_runtime_us = G_TASK_XXXX_RUNTIME_US_INI;

	/* Print out: Task Initialized */
	LOGGER_INFO(" ");
	LOGGER_INFO("%s is running - Tick [mS] = %3d", pcTaskGetName(NULL), (int)xTaskGetTickCount());

	/* As per most tasks, this task is implemented in an infinite loop. */
	for (;;)
	{
		/* Update Task Counter */
		g_task_xxxx_rx_cnt++;

		cycle_counter_reset();

		HAL_GPIO_TogglePin(LED_A_PORT, LED_A_PIN);

		g_task_xxxx_rx_runtime_us = cycle_counter_get_time_us();

    	/* Print out: Wait 250mS */
		LOGGER_INFO(p_task_uart_rx_wait_250mS);
		vTaskDelay(TASK_XXXX_DEL_MAX);
	}
}

/********************** end of file ******************************************/
