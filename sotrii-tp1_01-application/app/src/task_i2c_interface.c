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
#include "task_i2c_interface.h"
#include "pcf8574_lcd.h"

/********************** macros and definitions *******************************/

/********************** internal data declaration ****************************/

/********************** internal data declaration ****************************/
task_i2c_dta_t task_i2c_dta;

/********************** internal functions declaration ***********************/
static void i2c_if_update_wcet(uint32_t elapsed_us, uint32_t *p_wcet_us);

/********************** internal data definition *****************************/

/********************** external data declaration ****************************/
uint32_t g_i2c_if_open_wcet_us;
uint32_t g_i2c_if_release_wcet_us;
uint32_t g_i2c_if_write_wcet_us;
uint32_t g_i2c_if_read_wcet_us;
uint32_t g_i2c_if_ioctl_wcet_us;

/********************** external functions definition ************************/
static void i2c_if_update_wcet(uint32_t elapsed_us, uint32_t *p_wcet_us)
{
	if (elapsed_us > *p_wcet_us)
	{
		*p_wcet_us = elapsed_us;
	}
}

/*
 * Paso 06 - Device Driver I2C FreeRTOS
 * ------------------------------------
 * Patron: Gatekeeper (task_i2c_tx / task_i2c_rx) + Synchronous API.
 * Periferico: HAL_I2C_Master_Transmit/Receive con polling (HAL_MAX_DELAY).
 * Almacenamiento: FreeRTOS Queues.
 *
 * WCET medido con DWT @ 84 MHz (SystemCoreClock). Variables g_i2c_if_*_wcet_us
 * registran el maximo observado en depuracion (Live Expressions / Watch).
 * Ver mediciones consolidadas en sotrii-tp1_01-application.md
 */

/* Interface functions */
void open_i2c(I2C_HandleTypeDef *h_i2c_device)
{
	BaseType_t ret;
	uint32_t elapsed_us;
	task_i2c_dta_t *p_task_i2c_dta = &task_i2c_dta;

	cycle_counter_reset();

	p_task_i2c_dta->device_id = h_i2c_device;

    /* Colas del device driver (Gatekeeper pattern) */
	p_task_i2c_dta->queue_tx = xQueueCreate(TASK_I2C_QUEUE_TX_LEN, sizeof(task_i2c_tx_dta_t));
	configASSERT(NULL != p_task_i2c_dta->queue_tx);
	vQueueAddToRegistry(p_task_i2c_dta->queue_tx, "Task I2C Tx Queue Handle");

	p_task_i2c_dta->queue_tx_sync = xQueueCreate(TASK_I2C_QUEUE_TX_SYNC_LEN, sizeof(uint8_t));
	configASSERT(NULL != p_task_i2c_dta->queue_tx_sync);
	vQueueAddToRegistry(p_task_i2c_dta->queue_tx_sync, "Task I2C Tx Sync Queue Handle");

	p_task_i2c_dta->queue_rx_req = xQueueCreate(TASK_I2C_QUEUE_RX_REQ_LEN, sizeof(task_i2c_rx_dta_t));
	configASSERT(NULL != p_task_i2c_dta->queue_rx_req);
	vQueueAddToRegistry(p_task_i2c_dta->queue_rx_req, "Task I2C Rx Req Queue Handle");

	p_task_i2c_dta->queue_rx = xQueueCreate(TASK_I2C_QUEUE_RX_LEN, sizeof(uint8_t));
	configASSERT(NULL != p_task_i2c_dta->queue_rx);
	vQueueAddToRegistry(p_task_i2c_dta->queue_rx, "Task I2C Rx Queue Handle");

    /* Tareas Gatekeeper: una para TX y otra para RX sobre el canal I2C */
    ret = xTaskCreate(task_i2c_tx, "Task I2C Tx", (configMINIMAL_STACK_SIZE),
					  (void *)p_task_i2c_dta,
					  (tskIDLE_PRIORITY + 1ul), &p_task_i2c_dta->task_tx);
    configASSERT(pdPASS == ret);

    ret = xTaskCreate(task_i2c_rx, "Task I2C Rx", (configMINIMAL_STACK_SIZE),
    				  (void *)p_task_i2c_dta,
					  (tskIDLE_PRIORITY + 1ul), &p_task_i2c_dta->task_rx);
    configASSERT(pdPASS == ret);

	elapsed_us = cycle_counter_get_time_us();
	i2c_if_update_wcet(elapsed_us, &g_i2c_if_open_wcet_us);
}

void release_i2c(I2C_HandleTypeDef *h_i2c_device)
{
	uint32_t elapsed_us;
	task_i2c_dta_t *p_task_i2c_dta = &task_i2c_dta;

	cycle_counter_reset();

	if (p_task_i2c_dta->device_id == h_i2c_device)
	{
		vTaskDelete(p_task_i2c_dta->task_tx);
		vTaskDelete(p_task_i2c_dta->task_rx);

	    vQueueUnregisterQueue(p_task_i2c_dta->queue_tx);
		vQueueDelete(p_task_i2c_dta->queue_tx);
	    vQueueUnregisterQueue(p_task_i2c_dta->queue_tx_sync);
		vQueueDelete(p_task_i2c_dta->queue_tx_sync);
	    vQueueUnregisterQueue(p_task_i2c_dta->queue_rx_req);
		vQueueDelete(p_task_i2c_dta->queue_rx_req);
	    vQueueUnregisterQueue(p_task_i2c_dta->queue_rx);
		vQueueDelete(p_task_i2c_dta->queue_rx);

		p_task_i2c_dta->device_id = NULL;
		p_task_i2c_dta->task_tx = NULL;
		p_task_i2c_dta->task_rx = NULL;
		p_task_i2c_dta->queue_tx = NULL;
		p_task_i2c_dta->queue_tx_sync = NULL;
		p_task_i2c_dta->queue_rx_req = NULL;
		p_task_i2c_dta->queue_rx = NULL;
	}

	elapsed_us = cycle_counter_get_time_us();
	i2c_if_update_wcet(elapsed_us, &g_i2c_if_release_wcet_us);
}

HAL_StatusTypeDef write_i2c(I2C_HandleTypeDef *h_i2c_device, uint16_t dev_address, uint8_t dev_data)
{
	HAL_StatusTypeDef hal_status = HAL_ERROR;
	uint32_t elapsed_us;
	task_i2c_dta_t *p_task_i2c_dta = &task_i2c_dta;

	cycle_counter_reset();

	if (p_task_i2c_dta->device_id == h_i2c_device)
	{
		task_i2c_tx_dta_t task_i2c_tx_dta;
		uint8_t sync_status;

		task_i2c_tx_dta.address = dev_address;
		task_i2c_tx_dta.data = dev_data;

		/* Synchronous: encolar peticion y bloquear hasta confirmacion TX */
		xQueueSend(p_task_i2c_dta->queue_tx, &task_i2c_tx_dta, portMAX_DELAY);
		xQueueReceive(p_task_i2c_dta->queue_tx_sync, &sync_status, portMAX_DELAY);

		hal_status = (HAL_StatusTypeDef)sync_status;
	}

	elapsed_us = cycle_counter_get_time_us();
	i2c_if_update_wcet(elapsed_us, &g_i2c_if_write_wcet_us);

	return hal_status;
}

uint8_t read_i2c(I2C_HandleTypeDef *h_i2c_device, uint16_t dev_address)
{
	uint8_t dev_data = 0u;
	uint32_t elapsed_us;
	task_i2c_dta_t *p_task_i2c_dta = &task_i2c_dta;

	cycle_counter_reset();

	if (p_task_i2c_dta->device_id == h_i2c_device)
	{
		task_i2c_rx_dta_t task_i2c_rx_dta;

		task_i2c_rx_dta.address = dev_address;

		/* Synchronous: encolar peticion RX y bloquear hasta recibir dato */
		xQueueSend(p_task_i2c_dta->queue_rx_req, &task_i2c_rx_dta, portMAX_DELAY);
		xQueueReceive(p_task_i2c_dta->queue_rx, &dev_data, portMAX_DELAY);
	}

	elapsed_us = cycle_counter_get_time_us();
	i2c_if_update_wcet(elapsed_us, &g_i2c_if_read_wcet_us);

	return dev_data;
}

HAL_StatusTypeDef ioctl_i2c(I2C_HandleTypeDef *h_i2c_device, task_i2c_ioctl_cmd_t cmd)
{
	HAL_StatusTypeDef hal_status = HAL_ERROR;
	uint32_t elapsed_us;
	task_i2c_dta_t *p_task_i2c_dta = &task_i2c_dta;

	cycle_counter_reset();

	if (p_task_i2c_dta->device_id == h_i2c_device)
	{
		switch (cmd)
		{
			case I2C_IOCTL_IS_READY:
				hal_status = HAL_I2C_IsDeviceReady(h_i2c_device, (LCD_DIR << 1), 3u, HAL_MAX_DELAY);
				break;

			case I2C_IOCTL_RESET_BUS:
				hal_status = HAL_I2C_DeInit(h_i2c_device);
				if (HAL_OK == hal_status)
				{
					hal_status = HAL_I2C_Init(h_i2c_device);
				}
				break;

			default:
				hal_status = HAL_ERROR;
				break;
		}
	}

	elapsed_us = cycle_counter_get_time_us();
	i2c_if_update_wcet(elapsed_us, &g_i2c_if_ioctl_wcet_us);

	return hal_status;
}

void i2c_if_wcet_report(void)
{
	extern uint32_t g_task_i2c_tx_runtime_us;
	extern uint32_t g_task_i2c_rx_runtime_us;

	LOGGER_INFO(" ");
	LOGGER_INFO("=== WCET Funciones Interfaz I2C [us] ===");
	LOGGER_INFO("  open_i2c()    : %lu", g_i2c_if_open_wcet_us);
	LOGGER_INFO("  release_i2c() : %lu", g_i2c_if_release_wcet_us);
	LOGGER_INFO("  write_i2c()   : %lu", g_i2c_if_write_wcet_us);
	LOGGER_INFO("  read_i2c()    : %lu", g_i2c_if_read_wcet_us);
	LOGGER_INFO("  ioctl_i2c()   : %lu", g_i2c_if_ioctl_wcet_us);
	LOGGER_INFO("=== WCET Gatekeeper I2C [us] ===");
	LOGGER_INFO("  task_i2c_tx   : %lu", g_task_i2c_tx_runtime_us);
	LOGGER_INFO("  task_i2c_rx   : %lu", g_task_i2c_rx_runtime_us);
}

/********************** end of file ******************************************/
