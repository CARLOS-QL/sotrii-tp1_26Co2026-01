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
#include <ctype.h>

/* Demo includes */
#include "logger.h"
#include "dwt.h"

/* Application & Tasks includes */
#include "board.h"
#include "app.h"
#include "task_uart_interface.h"

/********************** macros and definitions *******************************/
#define G_TASK_RECEIVER_CNT_INI			0ul

#define TASK_RECEIVER_CMD_MAX_LEN		(8u)
#define TASK_RECEIVER_RX_WAIT_MS		(100u)
#define TASK_RECEIVER_LOG_EVERY		(25ul)
#define TASK_RECEIVER_UART_REPLY_RETRY	(30u)
#define TASK_RECEIVER_STARTUP_DEL		(pdMS_TO_TICKS(300ul))

#define TASK_RECEIVER_CMD_ON			"ON"
#define TASK_RECEIVER_CMD_OFF			"OFF"
#define TASK_RECEIVER_RSP_ON			"encendido\r\n"
#define TASK_RECEIVER_RSP_OFF			"apagado\r\n"
#define TASK_RECEIVER_RSP_READY			"UART LISTO. CMD: ON / OFF + ENTER\r\n"
#define TASK_RECEIVER_RSP_INVALID		"CMD INVALIDO\r\n"

static const char * const s_valid_cmds[] =
{
	TASK_RECEIVER_CMD_ON,
	TASK_RECEIVER_CMD_OFF,
	NULL
};

/********************** internal data declaration ****************************/

/********************** internal functions declaration ***********************/
static void receiver_uart_reply(const char *p_msg);
static void receiver_reset_buffer(char *p_cmd, uint8_t *p_cmd_len);
static void receiver_trim_cmd(char *p_cmd);
static BaseType_t receiver_cmd_match(const char *p_cmd, const char *p_expected);
static BaseType_t receiver_is_valid_prefix(const char *p_cmd);
static BaseType_t receiver_process_command(const char *p_cmd);
static void receiver_flush_buffer(char *p_cmd, uint8_t *p_cmd_len);
static void receiver_handle_byte(char *p_cmd, uint8_t *p_cmd_len, uint8_t rx_byte);

/********************** internal data definition *****************************/
const char *p_task_receiver_alive		= "   ==> Task RECEIVER - alive";
const char *p_task_receiver_cmd_on		= "   ==> Task RECEIVER - CMD ON -> encendido";
const char *p_task_receiver_cmd_off	= "   ==> Task RECEIVER - CMD OFF -> apagado";
const char *p_task_receiver_cmd_bad	= "   ==> Task RECEIVER - CMD invalido";

/********************** external data declaration ****************************/
uint32_t g_task_receiver_cnt;

/********************** external functions definition ************************/
static void receiver_uart_reply(const char *p_msg)
{
	uint8_t retry = 0u;
	HAL_StatusTypeDef hal_status = HAL_BUSY;

	while ((HAL_OK != hal_status) && (retry < TASK_RECEIVER_UART_REPLY_RETRY))
	{
		hal_status = write_uart(&huart2, (const uint8_t *)p_msg, (uint16_t)strlen(p_msg));

		if (HAL_OK != hal_status)
		{
			vTaskDelay(pdMS_TO_TICKS(2ul));
			retry++;
		}
	}
}

static void receiver_reset_buffer(char *p_cmd, uint8_t *p_cmd_len)
{
	*p_cmd_len = 0u;
	p_cmd[0] = '\0';
}

static void receiver_trim_cmd(char *p_cmd)
{
	char *p_start = p_cmd;
	char *p_end = NULL;
	size_t len = 0u;

	while (('\0' != *p_start) && isspace((unsigned char)*p_start))
	{
		p_start++;
	}

	if (p_start != p_cmd)
	{
		(void)memmove(p_cmd, p_start, strlen(p_start) + 1u);
	}

	len = strlen(p_cmd);
	while ((0u < len) && isspace((unsigned char)p_cmd[len - 1u]))
	{
		p_cmd[len - 1u] = '\0';
		len--;
	}

	p_end = p_cmd;
	while ('\0' != *p_end)
	{
		*p_end = (char)toupper((unsigned char)*p_end);
		p_end++;
	}
}

static BaseType_t receiver_cmd_match(const char *p_cmd, const char *p_expected)
{
	size_t idx = 0u;

	while (('\0' != p_cmd[idx]) && ('\0' != p_expected[idx]))
	{
		if (toupper((unsigned char)p_cmd[idx]) !=
			toupper((unsigned char)p_expected[idx]))
		{
			return pdFALSE;
		}
		idx++;
	}

	return ((('\0' == p_cmd[idx]) && ('\0' == p_expected[idx])) ? pdTRUE : pdFALSE);
}

static BaseType_t receiver_is_valid_prefix(const char *p_cmd)
{
	size_t cmd_len = strlen(p_cmd);
	uint32_t idx = 0u;

	while (NULL != s_valid_cmds[idx])
	{
		size_t valid_len = strlen(s_valid_cmds[idx]);

		if (cmd_len <= valid_len)
		{
			if (0 == strncmp(s_valid_cmds[idx], p_cmd, cmd_len))
			{
				return pdTRUE;
			}
		}

		idx++;
	}

	return pdFALSE;
}

static BaseType_t receiver_process_command(const char *p_cmd)
{
	char cmd_buffer[TASK_RECEIVER_CMD_MAX_LEN];

	(void)strncpy(cmd_buffer, p_cmd, sizeof(cmd_buffer) - 1u);
	cmd_buffer[sizeof(cmd_buffer) - 1u] = '\0';
	receiver_trim_cmd(cmd_buffer);

	if (pdTRUE == receiver_cmd_match(cmd_buffer, TASK_RECEIVER_CMD_ON))
	{
		HAL_GPIO_WritePin(LED_A_PORT, LED_A_PIN, LED_A_ON);
		receiver_uart_reply(TASK_RECEIVER_RSP_ON);
		LOGGER_INFO(p_task_receiver_cmd_on);
		return pdTRUE;
	}

	if (pdTRUE == receiver_cmd_match(cmd_buffer, TASK_RECEIVER_CMD_OFF))
	{
		HAL_GPIO_WritePin(LED_A_PORT, LED_A_PIN, LED_A_OFF);
		receiver_uart_reply(TASK_RECEIVER_RSP_OFF);
		LOGGER_INFO(p_task_receiver_cmd_off);
		return pdTRUE;
	}

	return pdFALSE;
}

static void receiver_flush_buffer(char *p_cmd, uint8_t *p_cmd_len)
{
	if (0u < *p_cmd_len)
	{
		p_cmd[*p_cmd_len] = '\0';

		if (pdFALSE == receiver_process_command(p_cmd))
		{
			receiver_uart_reply(TASK_RECEIVER_RSP_INVALID);
			LOGGER_INFO(p_task_receiver_cmd_bad);
		}
	}

	receiver_reset_buffer(p_cmd, p_cmd_len);
}

static void receiver_handle_byte(char *p_cmd, uint8_t *p_cmd_len, uint8_t rx_byte)
{
	if (('\r' == rx_byte) || ('\n' == rx_byte))
	{
		receiver_flush_buffer(p_cmd, p_cmd_len);
		return;
	}

	if (('\b' == rx_byte) || (127u == rx_byte))
	{
		if (0u < *p_cmd_len)
		{
			(*p_cmd_len)--;
			p_cmd[*p_cmd_len] = '\0';
		}
		return;
	}

	if (*p_cmd_len >= (TASK_RECEIVER_CMD_MAX_LEN - 1u))
	{
		receiver_reset_buffer(p_cmd, p_cmd_len);
	}

	p_cmd[*p_cmd_len] = (char)rx_byte;
	(*p_cmd_len)++;
	p_cmd[*p_cmd_len] = '\0';

	if (pdFALSE == receiver_is_valid_prefix(p_cmd))
	{
		receiver_reset_buffer(p_cmd, p_cmd_len);

		if ('O' == (char)toupper((unsigned char)rx_byte))
		{
			p_cmd[0] = (char)rx_byte;
			*p_cmd_len = 1u;
			p_cmd[1] = '\0';
		}
	}
}

/* Task thread */
void task_receiver(void *parameters)
{
	uint8_t rx_byte = 0u;
	char cmd_buffer[TASK_RECEIVER_CMD_MAX_LEN];
	uint8_t cmd_len = 0u;

	UNUSED(parameters);

	g_task_receiver_cnt = G_TASK_RECEIVER_CNT_INI;
	receiver_reset_buffer(cmd_buffer, &cmd_len);

	LOGGER_INFO(" ");
	LOGGER_INFO("  %s is running - Tick [mS] = %lu", pcTaskGetName(NULL), xTaskGetTickCount());

	vTaskDelay(TASK_RECEIVER_STARTUP_DEL);
	(void)ioctl_uart(&huart2, UART_IOCTL_ARM_RX);
	receiver_uart_reply(TASK_RECEIVER_RSP_READY);
	LOGGER_INFO("  Task RECEIVER - UART listo (ON/OFF + ENTER)");

	for (;;)
	{
		g_task_receiver_cnt++;

		while (pdTRUE == read_uart_wait(&huart2, &rx_byte,
										pdMS_TO_TICKS(TASK_RECEIVER_RX_WAIT_MS)))
		{
			receiver_handle_byte(cmd_buffer, &cmd_len, rx_byte);
		}

		if (0u == (g_task_receiver_cnt % TASK_RECEIVER_LOG_EVERY))
		{
			LOGGER_INFO("%s - cnt=%lu", p_task_receiver_alive, g_task_receiver_cnt);
		}
	}
}


