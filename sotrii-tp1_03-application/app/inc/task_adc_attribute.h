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

#ifndef TASK_ADC_ATTRIBUTE_H_
#define TASK_ADC_ATTRIBUTE_H_

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
#define TASK_ADC_QUEUE_IN_LEN		(4u)
#define TASK_ADC_QUEUE_OUT_LEN		(1u)

#define TASK_ADC_DMA_BUFFER_LEN		(1u)

/********************** typedef **********************************************/
/* ADC Device Driver - data structures (Paso 06)
 * Patron Gatekeeper: colas Input/Output Spooler separan la API del ADC.
 * Patron Latest Input Only (LIO): output spooler profundidad 1 + xQueueOverwrite.
 * Periferico: HAL_ADC_Start_DMA (DMA2 Stream0).
 * Memoria: colas estaticas (xQueueCreateStatic). */

typedef enum
{
	ADC_IOCTL_FLUSH = 0,
	ADC_IOCTL_START_SAMPLING,
	ADC_IOCTL_STOP_SAMPLING
} task_adc_ioctl_cmd_t;

typedef enum
{
	ADC_IN_CMD_SAMPLE = 0,
	ADC_IN_CMD_START,
	ADC_IN_CMD_STOP,
	ADC_IN_CMD_FLUSH
} task_adc_in_cmd_t;

typedef struct
{
	task_adc_in_cmd_t	cmd;
} task_adc_in_dta_t;

typedef struct
{
	ADC_HandleTypeDef *	device_id;
	uint32_t			adc_channel;

	TaskHandle_t		task_adc;
	QueueHandle_t		queue_in;
	QueueHandle_t		queue_out;

	SemaphoreHandle_t	sem_dma_done;

	uint16_t			dma_buffer[TASK_ADC_DMA_BUFFER_LEN];
	BaseType_t			sampling_active;

	/* Static queue storage */
	uint8_t				queue_in_storage[TASK_ADC_QUEUE_IN_LEN * sizeof(task_adc_in_dta_t)];
	StaticQueue_t		queue_in_struct;

	uint8_t				queue_out_storage[TASK_ADC_QUEUE_OUT_LEN * sizeof(uint16_t)];
	StaticQueue_t		queue_out_struct;

	StaticSemaphore_t	sem_dma_done_struct;
} task_adc_dta_t;

/********************** external data declaration ****************************/
extern task_adc_dta_t task_adc_dta;

/********************** external functions declaration ***********************/

/********************** End of CPP guard *************************************/
#ifdef __cplusplus
}
#endif

#endif /* TASK_ADC_ATTRIBUTE_H_ */

/********************** end of file ******************************************/
