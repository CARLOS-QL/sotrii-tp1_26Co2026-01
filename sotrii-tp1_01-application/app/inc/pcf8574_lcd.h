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

#ifndef PCF8574_LCD_H_
#define PCF8574_LCD_H_

/********************** CPP guard ********************************************/
#ifdef __cplusplus
extern "C" {
#endif

/********************** inclusions *******************************************/
#include "stm32f4xx_hal.h"

/********************** macros ***********************************************/
/* Serial LCD I2C Module–PCF8574 + HD44780 (16x2)
 * Mapa de pines (estandar Arduino / LiquidCrystal_I2C, modulo 0x27 mas comun):
 *   P0 = RS, P1 = RW, P2 = EN, P3 = Backlight, P4-P7 = D4-D7
 * Si su modulo usa otro cableado (P7 = BL), cambie PCF8574_LCD_PINOUT. */
#define PCF8574_LCD_PINOUT_ARDUINO	(0)
#define PCF8574_LCD_PINOUT_ALSELECTRO	(1)
#define PCF8574_LCD_PINOUT			PCF8574_LCD_PINOUT_ARDUINO

#define LCD_DIR						(0x27u)
#define PCF8574_LCD_ADDR			LCD_DIR
#define PCF8574_LCD_ADDR_ALT		(0x3Fu)
#define PCF8574_LCD_COLS			(16u)
#define PCF8574_LCD_ROWS			(2u)

#if (PCF8574_LCD_PINOUT == PCF8574_LCD_PINOUT_ARDUINO)
#define PCF8574_LCD_RS				(0x01u)
#define PCF8574_LCD_RW				(0x02u)
#define PCF8574_LCD_EN				(0x04u)
#define PCF8574_LCD_BL				(0x08u)
#else
#define PCF8574_LCD_RS				(0x01u << 4)
#define PCF8574_LCD_RW				(0x01u << 5)
#define PCF8574_LCD_EN				(0x01u << 6)
#define PCF8574_LCD_BL				(0x01u << 7)
#endif

/********************** typedef **********************************************/
typedef struct
{
	I2C_HandleTypeDef *	hi2c;
	uint16_t			address;
	uint8_t				backlight;
} pcf8574_lcd_t;

/********************** external data declaration ****************************/
extern pcf8574_lcd_t g_pcf8574_lcd;

/********************** external functions declaration ***********************/
extern void lcd_init(pcf8574_lcd_t *lcd, I2C_HandleTypeDef *hi2c, uint16_t address);
extern void lcd_startup(pcf8574_lcd_t *lcd, I2C_HandleTypeDef *hi2c);
extern void lcd_backlight_on(pcf8574_lcd_t *lcd);
extern void lcd_backlight_off(pcf8574_lcd_t *lcd);
extern void lcd_clear(pcf8574_lcd_t *lcd);
extern void lcd_home(pcf8574_lcd_t *lcd);
extern void lcd_set_cursor(pcf8574_lcd_t *lcd, uint8_t row, uint8_t col);
extern void lcd_write_char(pcf8574_lcd_t *lcd, char chr);
extern void lcd_write_string(pcf8574_lcd_t *lcd, const char *str);
extern void lcd_write_string_at(pcf8574_lcd_t *lcd, uint8_t row, uint8_t col, const char *str);
extern void lcd_write_line(pcf8574_lcd_t *lcd, uint8_t row, const char *str);
extern void lcd_write_data_at(pcf8574_lcd_t *lcd, uint8_t row, uint8_t col, const char *label, uint32_t value);
extern void lcd_write_hex8_at(pcf8574_lcd_t *lcd, uint8_t row, uint8_t col, const char *label, uint8_t value);

/********************** End of CPP guard *************************************/
#ifdef __cplusplus
}
#endif

#endif /* PCF8574_LCD_H_ */

/********************** end of file ******************************************/
