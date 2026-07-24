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

#ifndef TASK_UART_INTERFACE_H_
#define TASK_UART_INTERFACE_H_

/********************** CPP guard ********************************************/
#ifdef __cplusplus
extern "C" {
#endif

/********************** inclusions *******************************************/
#include "task_uart_attribute.h"

/********************** macros ***********************************************/

/********************** typedef **********************************************/

/********************** external data declaration ****************************/
extern uint32_t g_uart_if_open_wcet_us;
extern uint32_t g_uart_if_release_wcet_us;
extern uint32_t g_uart_if_write_wcet_us;
extern uint32_t g_uart_if_read_wcet_us;
extern uint32_t g_uart_if_ioctl_wcet_us;
extern volatile uint32_t g_uart_rx_isr_cnt;

/********************** external functions declaration ***********************/
extern void open_uart(UART_HandleTypeDef *h_uart_device);
extern void release_uart(UART_HandleTypeDef *h_uart_device);

extern HAL_StatusTypeDef write_uart(UART_HandleTypeDef *h_uart_device,
									const uint8_t *p_data,
									uint16_t length);

extern BaseType_t read_uart(UART_HandleTypeDef *h_uart_device, uint8_t *p_data);

extern BaseType_t read_uart_wait(UART_HandleTypeDef *h_uart_device,
								 uint8_t *p_data,
								 TickType_t timeout_ticks);

extern HAL_StatusTypeDef ioctl_uart(UART_HandleTypeDef *h_uart_device,
									task_uart_ioctl_cmd_t cmd);

extern void uart_if_wcet_report(void);

extern void uart_tx_cplt_notify_from_isr(BaseType_t *p_higher_priority_woken);
extern void uart_rx_cplt_notify_from_isr(uint8_t rx_byte,
										 BaseType_t *p_higher_priority_woken);
extern void uart_error_notify_from_isr(BaseType_t *p_higher_priority_woken);

/********************** End of CPP guard *************************************/
#ifdef __cplusplus
}
#endif

#endif /* TASK_UART_INTERFACE_H_ */

/********************** end of file ******************************************/
