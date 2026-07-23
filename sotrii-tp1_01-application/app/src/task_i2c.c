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
#include "task_i2c.h"
#include "task_i2c_attribute.h"

/********************** macros and definitions *******************************/
#define G_TASK_I2C_CNT_INI			0ul
#define G_TASK_I2C_RUNTIME_US_INI	0ul

/********************** internal data declaration ****************************/

/********************** internal data declaration ****************************/

/********************** internal functions declaration ***********************/

/********************** internal data definition *****************************/

/********************** external data declaration ****************************/
uint32_t g_task_i2c_tx_cnt;
uint32_t g_task_i2c_tx_runtime_us;

uint32_t g_task_i2c_rx_cnt;
uint32_t g_task_i2c_rx_runtime_us;

/********************** external functions definition ************************/
/*
 * Gatekeeper TX: unica tarea autorizada a transmitir por I2C1.
 * Recibe peticiones por queue_tx, transfiere con HAL (polling) y
 * confirma por queue_tx_sync (patron Synchronous).
 *
 * WCET gatekeeper TX (HAL_I2C_Master_Transmit 1 byte @ 100 kHz): ~ 95 us
 */
void task_i2c_tx(void *parameters)
{
	g_task_i2c_tx_cnt = G_TASK_I2C_CNT_INI;
	g_task_i2c_tx_runtime_us = G_TASK_I2C_RUNTIME_US_INI;

	task_i2c_dta_t *p_task_i2c_dta = (task_i2c_dta_t *)parameters;

	/* Serial LCD I2C Module–PCF8574
	 * https://alselectro.wordpress.com/2016/05/12/serial-lcd-i2c-module-pcf8574/
	 * https://www.ti.com/product/PCF8574
 	 * i2c1_tx_address_rd_wr = ((address base | jumper less address) << 1) | /write
 	 */

	LOGGER_INFO(" ");
	LOGGER_INFO("%s is running - Tick [mS] = %3d", pcTaskGetName(NULL), (int)xTaskGetTickCount());

	for (;;)
	{
		HAL_StatusTypeDef hal_status;
		task_i2c_tx_dta_t task_i2c_tx_dta;
		uint8_t sync_status;

		g_task_i2c_tx_cnt++;

		xQueueReceive(p_task_i2c_dta->queue_tx, &task_i2c_tx_dta, portMAX_DELAY);

		cycle_counter_reset();

		hal_status = HAL_I2C_Master_Transmit(p_task_i2c_dta->device_id,
											 (task_i2c_tx_dta.address << 1),
											 &task_i2c_tx_dta.data,
											 sizeof(task_i2c_tx_dta.data),
											 HAL_MAX_DELAY);

		sync_status = (uint8_t)hal_status;
		xQueueSend(p_task_i2c_dta->queue_tx_sync, &sync_status, portMAX_DELAY);

		g_task_i2c_tx_runtime_us = cycle_counter_get_time_us();
	}
}

/*
 * Gatekeeper RX: unica tarea autorizada a recibir por I2C1.
 * Recibe peticiones por queue_rx_req, transfiere con HAL (polling) y
 * entrega el dato por queue_rx (patron Synchronous).
 *
 * WCET gatekeeper RX (HAL_I2C_Master_Receive 1 byte @ 100 kHz): ~ 92 us
 */
void task_i2c_rx(void *parameters)
{
	g_task_i2c_rx_cnt = G_TASK_I2C_CNT_INI;
	g_task_i2c_rx_runtime_us = G_TASK_I2C_RUNTIME_US_INI;

	task_i2c_dta_t *p_task_i2c_dta = (task_i2c_dta_t *)parameters;

	LOGGER_INFO(" ");
	LOGGER_INFO("%s is running - Tick [mS] = %3d", pcTaskGetName(NULL), (int)xTaskGetTickCount());

	for (;;)
	{
		HAL_StatusTypeDef hal_status;
		task_i2c_rx_dta_t task_i2c_rx_dta;
		uint8_t dev_data = 0u;

		g_task_i2c_rx_cnt++;

		xQueueReceive(p_task_i2c_dta->queue_rx_req, &task_i2c_rx_dta, portMAX_DELAY);

		cycle_counter_reset();

		hal_status = HAL_I2C_Master_Receive(p_task_i2c_dta->device_id,
											(task_i2c_rx_dta.address << 1),
											&dev_data,
											sizeof(dev_data),
											HAL_MAX_DELAY);

		if (HAL_OK != hal_status)
		{
			dev_data = 0u;
		}

		xQueueSend(p_task_i2c_dta->queue_rx, &dev_data, portMAX_DELAY);

		g_task_i2c_rx_runtime_us = cycle_counter_get_time_us();
	}
}

/********************** end of file ******************************************/
