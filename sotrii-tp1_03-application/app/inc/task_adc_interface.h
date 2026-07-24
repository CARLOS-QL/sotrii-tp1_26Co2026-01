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

#ifndef TASK_ADC_INTERFACE_H_
#define TASK_ADC_INTERFACE_H_

/********************** CPP guard ********************************************/
#ifdef __cplusplus
extern "C" {
#endif

/********************** inclusions *******************************************/
#include "task_adc_attribute.h"

/********************** macros ***********************************************/

/********************** typedef **********************************************/

/********************** external data declaration ****************************/
extern uint32_t g_adc_if_open_wcet_us;
extern uint32_t g_adc_if_release_wcet_us;
extern uint32_t g_adc_if_write_wcet_us;
extern uint32_t g_adc_if_read_wcet_us;
extern uint32_t g_adc_if_ioctl_wcet_us;
extern volatile uint32_t g_adc_dma_isr_cnt;

/********************** external functions declaration ***********************/
extern void open_adc(ADC_HandleTypeDef *h_adc_device);
extern void release_adc(ADC_HandleTypeDef *h_adc_device);

extern HAL_StatusTypeDef write_adc(ADC_HandleTypeDef *h_adc_device);

extern BaseType_t read_adc(ADC_HandleTypeDef *h_adc_device, uint16_t *p_value);

extern BaseType_t read_adc_wait(ADC_HandleTypeDef *h_adc_device,
								uint16_t *p_value,
								TickType_t timeout_ticks);

extern HAL_StatusTypeDef ioctl_adc(ADC_HandleTypeDef *h_adc_device,
								   task_adc_ioctl_cmd_t cmd);

extern void adc_if_wcet_report(void);

extern void adc_dma_cplt_notify_from_isr(BaseType_t *p_higher_priority_woken);

/********************** End of CPP guard *************************************/
#ifdef __cplusplus
}
#endif

#endif /* TASK_ADC_INTERFACE_H_ */

/********************** end of file ******************************************/
