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
#include "task_adc_attribute.h"
#include "task_adc_interface.h"

/********************** macros and definitions *******************************/
#define G_TASK_ADC_CNT_INI			0ul
#define G_TASK_ADC_RUNTIME_US_INI	0ul

/********************** internal data declaration ****************************/

/********************** internal functions declaration ***********************/
static void adc_gatekeeper_flush_output(task_adc_dta_t *p_task_adc_dta);
static void adc_gatekeeper_run_conversion(task_adc_dta_t *p_task_adc_dta);

/********************** internal data definition *****************************/

/********************** external data declaration ****************************/
uint32_t g_task_adc_cnt;
uint32_t g_task_adc_runtime_us;

/********************** external functions definition ************************/
static void adc_gatekeeper_flush_output(task_adc_dta_t *p_task_adc_dta)
{
	uint16_t dummy_value;

	while (pdPASS == xQueueReceive(p_task_adc_dta->queue_out, &dummy_value, 0))
	{
		/* vaciar output spooler LIO */
	}
}

static void adc_gatekeeper_run_conversion(task_adc_dta_t *p_task_adc_dta)
{
	HAL_StatusTypeDef hal_status;

	while (pdTRUE == xSemaphoreTake(p_task_adc_dta->sem_dma_done, 0))
	{
		/* descartar eventos previos del semaforo DMA */
	}

	hal_status = HAL_ADC_Start_DMA(p_task_adc_dta->device_id,
								   (uint32_t *)p_task_adc_dta->dma_buffer,
								   TASK_ADC_DMA_BUFFER_LEN);

	if (HAL_OK == hal_status)
	{
		(void)xSemaphoreTake(p_task_adc_dta->sem_dma_done, portMAX_DELAY);

		/* LIO: la muestra mas reciente reemplaza la anterior */
		(void)xQueueOverwrite(p_task_adc_dta->queue_out,
							  &p_task_adc_dta->dma_buffer[0]);
	}
}

/*
 * Gatekeeper ADC: unica tarea autorizada a operar el ADC1 via DMA.
 * Recibe como parametro task_adc_dta_t con device_id y adc_channel (PA0).
 */
void task_adc(void *parameters)
{
	task_adc_dta_t *p_task_adc_dta = (task_adc_dta_t *)parameters;
	task_adc_in_dta_t in_cmd;
	BaseType_t one_shot_pending = pdFALSE;

	g_task_adc_cnt = G_TASK_ADC_CNT_INI;
	g_task_adc_runtime_us = G_TASK_ADC_RUNTIME_US_INI;

	LOGGER_INFO(" ");
	LOGGER_INFO("%s is running - ADC CH %lu - Tick [mS] = %3d",
				pcTaskGetName(NULL),
				(unsigned long)p_task_adc_dta->adc_channel,
				(int)xTaskGetTickCount());

	for (;;)
	{
		g_task_adc_cnt++;

		cycle_counter_reset();

		if (pdTRUE == p_task_adc_dta->sampling_active)
		{
			adc_gatekeeper_run_conversion(p_task_adc_dta);

			while (pdPASS == xQueueReceive(p_task_adc_dta->queue_in, &in_cmd, 0))
			{
				switch (in_cmd.cmd)
				{
					case ADC_IN_CMD_STOP:
						p_task_adc_dta->sampling_active = pdFALSE;
						(void)HAL_ADC_Stop_DMA(p_task_adc_dta->device_id);
						break;

					case ADC_IN_CMD_FLUSH:
						adc_gatekeeper_flush_output(p_task_adc_dta);
						break;

					default:
						break;
				}
			}

			vTaskDelay(pdMS_TO_TICKS(1ul));
		}
		else if (pdTRUE == one_shot_pending)
		{
			adc_gatekeeper_run_conversion(p_task_adc_dta);
			one_shot_pending = pdFALSE;
		}
		else
		{
			xQueueReceive(p_task_adc_dta->queue_in, &in_cmd, portMAX_DELAY);

			switch (in_cmd.cmd)
			{
				case ADC_IN_CMD_START:
					p_task_adc_dta->sampling_active = pdTRUE;
					break;

				case ADC_IN_CMD_SAMPLE:
					one_shot_pending = pdTRUE;
					break;

				case ADC_IN_CMD_STOP:
					p_task_adc_dta->sampling_active = pdFALSE;
					(void)HAL_ADC_Stop_DMA(p_task_adc_dta->device_id);
					break;

				case ADC_IN_CMD_FLUSH:
					adc_gatekeeper_flush_output(p_task_adc_dta);
					break;

				default:
					break;
			}
		}

		g_task_adc_runtime_us = cycle_counter_get_time_us();
	}
}

/********************** end of file ******************************************/
