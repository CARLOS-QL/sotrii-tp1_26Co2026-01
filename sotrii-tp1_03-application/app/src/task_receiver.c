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
#include "task_adc_interface.h"

/********************** macros and definitions *******************************/
#define G_TASK_RECEIVER_CNT_INI	0ul

#define TASK_RECEIVER_DEL_ZERO		(pdMS_TO_TICKS(0ul))
#define TASK_RECEIVER_DEL_MAX		(pdMS_TO_TICKS(250ul))
#define TASK_RECEIVER_WCET_REPORT_EVERY	(20ul)

#define ADC_MAX_VALUE_12B			(4095u)

/********************** internal data declaration ****************************/

/********************** internal functions declaration ***********************/

/********************** internal data definition *****************************/
const char *p_task_receiver_wait_250mS		= "   ==> Task RECEIVER - Wait:   250mS";

/********************** external data declaration ****************************/
uint32_t g_task_receiver_cnt;

/********************** external functions definition ************************/
/* Task thread */
void task_receiver(void *parameters)
{
	uint16_t adc_raw = 0u;
	uint32_t adc_percent = 0u;
	BaseType_t adc_ok = pdFALSE;

	/* Prevent unused argument(s) compilation warning */
	UNUSED(parameters);

	/*  Declare & Initialize Task Function variables */
	g_task_receiver_cnt = G_TASK_RECEIVER_CNT_INI;

	/* Print out: Task Initialized */
	LOGGER_INFO(" ");
	LOGGER_INFO("  %s is running - Tick [mS] = %lu", pcTaskGetName(NULL), xTaskGetTickCount());
	LOGGER_INFO("  Potenciometro en PA0 (A0) - ADC1 CH0 - LIO + DMA");

	/* As per most tasks, this task is implemented in an infinite loop. */
	for (;;)
    {
		/* Update Task Counter */
		g_task_receiver_cnt++;

		if (0u == (g_task_receiver_cnt % TASK_RECEIVER_WCET_REPORT_EVERY))
		{
			adc_if_wcet_report();
		}

		(void)ioctl_adc(&hadc1, ADC_IOCTL_FLUSH);
		(void)write_adc(&hadc1);
		adc_ok = read_adc_wait(&hadc1, &adc_raw, pdMS_TO_TICKS(100u));

		if (pdTRUE == adc_ok)
		{
			adc_percent = ((uint32_t)adc_raw * 100u) / ADC_MAX_VALUE_12B;
			LOGGER_INFO("  ADC raw=%4u  (~%2lu%%)  cnt=%lu",
						adc_raw, adc_percent, g_task_receiver_cnt);
		}
		else
		{
			LOGGER_INFO("  ADC sin muestra nueva - cnt=%lu", g_task_receiver_cnt);
		}

    	/* Print out: Wait 250mS */
		LOGGER_INFO(p_task_receiver_wait_250mS);
		vTaskDelay(TASK_RECEIVER_DEL_MAX);
	}
}

/********************** end of file ******************************************/
