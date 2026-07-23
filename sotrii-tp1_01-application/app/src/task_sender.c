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
#include "task_i2c_interface.h"
#include "pcf8574_lcd.h"

/********************** macros and definitions *******************************/
#define G_TASK_SENDER_CNT_INI	0ul

#define TASK_SENDER_DEL_ZERO	(pdMS_TO_TICKS(0ul))
#define TASK_SENDER_DEL_MAX		(pdMS_TO_TICKS(250ul))

static BaseType_t s_lcd_ready = pdFALSE;

/********************** internal data declaration ****************************/

/********************** internal functions declaration ***********************/

/********************** internal data definition *****************************/
const char *p_task_sender_wait_250mS		= "   ==> Task SENDER - Wait:   250mS";

/********************** external data declaration ****************************/
uint32_t g_task_sender_cnt;

/********************** external functions definition ************************/
/* Task thread */
void task_sender(void *parameters)
{
	/* Prevent unused argument(s) compilation warning */
	UNUSED(parameters);

	/*  Declare & Initialize Task Function variables */
	g_task_sender_cnt = G_TASK_SENDER_CNT_INI;

	/* Print out: Task Initialized */
	LOGGER_INFO(" ");
	LOGGER_INFO("  %s is running - Tick [mS] = %lu", pcTaskGetName(NULL), xTaskGetTickCount());

	/* LCD @ LCD_DIR (0x27): debe iniciarse con scheduler activo */
	if (pdFALSE == s_lcd_ready)
	{
		lcd_startup(&g_pcf8574_lcd, &hi2c1);
		s_lcd_ready = pdTRUE;
		LOGGER_INFO("  LCD init OK @ 0x%02X", g_pcf8574_lcd.address);

		/* Medicion WCET read/ioctl (una vez, con LCD conectado @ LCD_DIR) */
		(void)ioctl_i2c(&hi2c1, I2C_IOCTL_IS_READY);
		(void)read_i2c(&hi2c1, LCD_DIR);
		i2c_if_wcet_report();
	}

	/* As per most tasks, this task is implemented in an infinite loop. */
	for (;;)
	{
		/* Update Task Counter */
		g_task_sender_cnt++;

		/* Mostrar dato en linea 1 (linea 0: "Hola Mundo") */
		g_lcd_dato = (uint8_t)(g_lcd_dato + 1u);
		lcd_write_hex8_at(&g_pcf8574_lcd, 1u, 0u, "Dato", g_lcd_dato);

    	/* Print out: Wait 250mS */
		LOGGER_INFO(p_task_sender_wait_250mS);
		vTaskDelay(TASK_SENDER_DEL_MAX);
	}
}

/********************** end of file ******************************************/
