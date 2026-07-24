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

#ifndef TASK_UART_ATTRIBUTE_H_
#define TASK_UART_ATTRIBUTE_H_

/********************** CPP guard ********************************************/
#ifdef __cplusplus
extern "C" {
#endif

/********************** inclusions *******************************************/
#include "stm32f4xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

/********************** macros ***********************************************/
#define TASK_UART_QUEUE_TX_IN_LEN		(16u)
#define TASK_UART_QUEUE_TX_OUT_LEN		(16u)
#define TASK_UART_QUEUE_RX_IN_LEN		(8u)
#define TASK_UART_QUEUE_RX_OUT_LEN		(32u)

/********************** typedef **********************************************/
/* UART Device Driver - data structures (Paso 06)
 * Patron Gatekeeper: colas Input/Output Spooler separan la API del UART.
 * Patron Asynchronous: la API encola y retorna sin esperar la transferencia.
 * Periferico: HAL_UART_Transmit_IT / HAL_UART_Receive_IT (interrupciones).
 * Memoria: buffers del spooler asignados con pvPortMalloc (dinamica). */

typedef enum
{
	UART_IOCTL_FLUSH_RX = 0,
	UART_IOCTL_FLUSH_TX,
	UART_IOCTL_ABORT_TX,
	UART_IOCTL_ARM_RX
} task_uart_ioctl_cmd_t;

typedef enum
{
	UART_SPOOLER_STATUS_OK = 0,
	UART_SPOOLER_STATUS_ERROR,
	UART_SPOOLER_STATUS_BUSY
} task_uart_spooler_status_t;

typedef enum
{
	UART_RX_CMD_ARM = 0,
	UART_RX_CMD_FLUSH
} task_uart_rx_cmd_t;

/* Bloque dinamico del spooler (buffer en heap FreeRTOS) */
typedef struct
{
	uint8_t *	p_buffer;
	uint16_t	length;
} task_uart_spooler_dta_t;

/* Output spooler TX: estado de finalizacion (async) */
typedef struct
{
	task_uart_spooler_status_t	status;
	uint16_t					bytes_done;
} task_uart_tx_out_dta_t;

/* Input spooler RX: comandos al gatekeeper RX */
typedef struct
{
	task_uart_rx_cmd_t	cmd;
} task_uart_rx_in_dta_t;

/* Instancia del device driver */
typedef struct
{
	UART_HandleTypeDef *	device_id;

	TaskHandle_t			task_tx;
	QueueHandle_t			queue_tx_in;
	QueueHandle_t			queue_tx_out;

	TaskHandle_t			task_rx;
	QueueHandle_t			queue_rx_in;
	QueueHandle_t			queue_rx_out;

	SemaphoreHandle_t		sem_tx_done;

	task_uart_spooler_dta_t	tx_active;
	uint8_t					rx_byte;
	BaseType_t				rx_armed;
} task_uart_dta_t;

/********************** external data declaration ****************************/
extern task_uart_dta_t task_uart_dta;

/********************** external functions declaration ***********************/

/********************** End of CPP guard *************************************/
#ifdef __cplusplus
}
#endif

#endif /* TASK_UART_ATTRIBUTE_H_ */

/********************** end of file ******************************************/
