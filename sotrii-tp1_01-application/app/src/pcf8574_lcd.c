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
#include <stdio.h>

#include "main.h"
#include "cmsis_os.h"
#include "task_i2c_interface.h"
#include "pcf8574_lcd.h"

/********************** macros and definitions *******************************/
#define LCD_CMD_MODE				(0u)
#define LCD_DATA_MODE				(1u)

#define LCD_CMD_CLEAR				(0x01u)
#define LCD_CMD_HOME				(0x02u)
#define LCD_CMD_ENTRY_MODE			(0x06u)
#define LCD_CMD_DISPLAY_ON			(0x0Cu)
#define LCD_CMD_FUNCTION_SET		(0x28u)
#define LCD_CMD_4BIT_INIT_1			(0x03u)
#define LCD_CMD_4BIT_INIT_2			(0x02u)

#define LCD_ROW1_OFFSET				(0x40u)

/********************** internal data declaration ****************************/
static SemaphoreHandle_t s_lcd_mutex;

/********************** internal functions declaration ***********************/
static void lcd_lock(void);
static void lcd_unlock(void);
static void lcd_delay_us(uint32_t us);
static void lcd_delay_ms(uint32_t ms);
static void lcd_expander_write(pcf8574_lcd_t *lcd, uint8_t data);
static void lcd_pulse_enable(pcf8574_lcd_t *lcd, uint8_t data);
static void lcd_write4bits(pcf8574_lcd_t *lcd, uint8_t value, uint8_t mode);
static void lcd_send(pcf8574_lcd_t *lcd, uint8_t value, uint8_t mode);
static void lcd_command(pcf8574_lcd_t *lcd, uint8_t cmd);
static uint16_t lcd_detect_address(I2C_HandleTypeDef *hi2c);

/********************** external data declaration ****************************/
pcf8574_lcd_t g_pcf8574_lcd;

/********************** internal functions definition ************************/
static void lcd_lock(void)
{
	if (NULL != s_lcd_mutex)
	{
		(void)xSemaphoreTake(s_lcd_mutex, portMAX_DELAY);
	}
}

static void lcd_unlock(void)
{
	if (NULL != s_lcd_mutex)
	{
		(void)xSemaphoreGive(s_lcd_mutex);
	}
}

static void lcd_delay_us(uint32_t us)
{
	uint32_t start = DWT->CYCCNT;
	uint32_t cycles = us * (SystemCoreClock / 1000000u);

	while ((DWT->CYCCNT - start) < cycles)
	{
	}
}

static void lcd_delay_ms(uint32_t ms)
{
	vTaskDelay(pdMS_TO_TICKS(ms));
}

static void lcd_expander_write(pcf8574_lcd_t *lcd, uint8_t data)
{
	data |= lcd->backlight;
	(void)write_i2c(lcd->hi2c, lcd->address, data);
}

static void lcd_pulse_enable(pcf8574_lcd_t *lcd, uint8_t data)
{
	lcd_expander_write(lcd, data | PCF8574_LCD_EN);
	lcd_delay_us(1u);
	lcd_expander_write(lcd, data & (uint8_t)(~PCF8574_LCD_EN));
	lcd_delay_us(50u);
}

static void lcd_write4bits(pcf8574_lcd_t *lcd, uint8_t value, uint8_t mode)
{
#if (PCF8574_LCD_PINOUT == PCF8574_LCD_PINOUT_ARDUINO)
	uint8_t data = (uint8_t)(value & 0xF0u);
#else
	uint8_t data = (uint8_t)((value & 0x0Fu) << 4);
#endif

	if (LCD_DATA_MODE == mode)
	{
		data |= PCF8574_LCD_RS;
	}

	lcd_pulse_enable(lcd, data);
}

static void lcd_send(pcf8574_lcd_t *lcd, uint8_t value, uint8_t mode)
{
#if (PCF8574_LCD_PINOUT == PCF8574_LCD_PINOUT_ARDUINO)
	lcd_write4bits(lcd, (uint8_t)(value & 0xF0u), mode);
	lcd_write4bits(lcd, (uint8_t)((value << 4) & 0xF0u), mode);
#else
	lcd_write4bits(lcd, (uint8_t)(value >> 4), mode);
	lcd_write4bits(lcd, (uint8_t)(value & 0x0Fu), mode);
#endif
}

static void lcd_command(pcf8574_lcd_t *lcd, uint8_t cmd)
{
	lcd_send(lcd, cmd, LCD_CMD_MODE);

	if ((LCD_CMD_CLEAR == cmd) || (LCD_CMD_HOME == cmd))
	{
		lcd_delay_ms(2u);
	}
}

static uint16_t lcd_detect_address(I2C_HandleTypeDef *hi2c)
{
	if (HAL_OK == HAL_I2C_IsDeviceReady(hi2c, (LCD_DIR << 1), 3u, 100u))
	{
		return LCD_DIR;
	}

	if (HAL_OK == HAL_I2C_IsDeviceReady(hi2c, (PCF8574_LCD_ADDR_ALT << 1), 3u, 100u))
	{
		return PCF8574_LCD_ADDR_ALT;
	}

	return LCD_DIR;
}

/********************** external functions definition ************************/
void lcd_startup(pcf8574_lcd_t *lcd, I2C_HandleTypeDef *hi2c)
{
	uint16_t address = lcd_detect_address(hi2c);

	lcd->hi2c = hi2c;
	lcd->address = address;
	lcd->backlight = PCF8574_LCD_BL;

	/* Encender backlight de inmediato (antes del init HD44780) */
	(void)write_i2c(hi2c, address, PCF8574_LCD_BL);

	lcd_init(lcd, hi2c, address);
	lcd_backlight_on(lcd);
	lcd_clear(lcd);
	lcd_delay_ms(5u);
	lcd_write_line(lcd, 0u, "Hola Mundo");
	lcd_write_line(lcd, 1u, "Dato: ---");
}
void lcd_init(pcf8574_lcd_t *lcd, I2C_HandleTypeDef *hi2c, uint16_t address)
{
	if (NULL == s_lcd_mutex)
	{
		s_lcd_mutex = xSemaphoreCreateMutex();
		configASSERT(NULL != s_lcd_mutex);
	}

	lcd->hi2c = hi2c;
	lcd->address = address;
	lcd->backlight = PCF8574_LCD_BL;

	lcd_lock();

	lcd_delay_ms(50u);

#if (PCF8574_LCD_PINOUT == PCF8574_LCD_PINOUT_ARDUINO)
	/* Init 4-bit: nibbles en bits 4-7 (LiquidCrystal_I2C / Arduino) */
	lcd_write4bits(lcd, 0x30u, LCD_CMD_MODE);
	lcd_delay_ms(5u);
	lcd_write4bits(lcd, 0x30u, LCD_CMD_MODE);
	lcd_delay_us(200u);
	lcd_write4bits(lcd, 0x30u, LCD_CMD_MODE);
	lcd_delay_us(200u);
	lcd_write4bits(lcd, 0x20u, LCD_CMD_MODE);
	lcd_delay_us(200u);
#else
	lcd_write4bits(lcd, LCD_CMD_4BIT_INIT_1, LCD_CMD_MODE);
	lcd_delay_ms(5u);
	lcd_write4bits(lcd, LCD_CMD_4BIT_INIT_1, LCD_CMD_MODE);
	lcd_delay_us(200u);
	lcd_write4bits(lcd, LCD_CMD_4BIT_INIT_1, LCD_CMD_MODE);
	lcd_delay_us(200u);
	lcd_write4bits(lcd, LCD_CMD_4BIT_INIT_2, LCD_CMD_MODE);
	lcd_delay_us(200u);
#endif

	lcd_command(lcd, LCD_CMD_FUNCTION_SET);
	lcd_command(lcd, LCD_CMD_DISPLAY_ON);
	lcd_command(lcd, LCD_CMD_CLEAR);
	lcd_command(lcd, LCD_CMD_ENTRY_MODE);

	lcd_delay_ms(2u);

	lcd_unlock();
}

void lcd_backlight_on(pcf8574_lcd_t *lcd)
{
	lcd_lock();
	lcd->backlight = PCF8574_LCD_BL;
	lcd_expander_write(lcd, 0u);
	lcd_unlock();
}

void lcd_backlight_off(pcf8574_lcd_t *lcd)
{
	lcd_lock();
	lcd->backlight = 0u;
	lcd_expander_write(lcd, 0u);
	lcd_unlock();
}

void lcd_clear(pcf8574_lcd_t *lcd)
{
	lcd_lock();
	lcd_command(lcd, LCD_CMD_CLEAR);
	lcd_unlock();
}

void lcd_home(pcf8574_lcd_t *lcd)
{
	lcd_lock();
	lcd_command(lcd, LCD_CMD_HOME);
	lcd_unlock();
}

void lcd_set_cursor(pcf8574_lcd_t *lcd, uint8_t row, uint8_t col)
{
	uint8_t address = col;

	if (row > 0u)
	{
		address = (uint8_t)(LCD_ROW1_OFFSET + col);
	}

	lcd_lock();
	lcd_command(lcd, (uint8_t)(0x80u | address));
	lcd_unlock();
}

void lcd_write_char(pcf8574_lcd_t *lcd, char chr)
{
	lcd_lock();
	lcd_send(lcd, (uint8_t)chr, LCD_DATA_MODE);
	lcd_unlock();
}

void lcd_write_string(pcf8574_lcd_t *lcd, const char *str)
{
	if (NULL == str)
	{
		return;
	}

	lcd_lock();

	while ('\0' != *str)
	{
		lcd_send(lcd, (uint8_t)*str, LCD_DATA_MODE);
		str++;
	}

	lcd_unlock();
}

void lcd_write_string_at(pcf8574_lcd_t *lcd, uint8_t row, uint8_t col, const char *str)
{
	lcd_set_cursor(lcd, row, col);
	lcd_write_string(lcd, str);
}

void lcd_write_line(pcf8574_lcd_t *lcd, uint8_t row, const char *str)
{
	uint8_t index = 0u;
	uint8_t address;

	if (NULL == str)
	{
		str = "";
	}

	if (row > 0u)
	{
		address = (uint8_t)(LCD_ROW1_OFFSET);
	}
	else
	{
		address = 0u;
	}

	lcd_lock();
	lcd_command(lcd, (uint8_t)(0x80u | address));

	while (('\0' != str[index]) && (index < PCF8574_LCD_COLS))
	{
		lcd_send(lcd, (uint8_t)str[index], LCD_DATA_MODE);
		index++;
	}

	while (index < PCF8574_LCD_COLS)
	{
		lcd_send(lcd, (uint8_t)' ', LCD_DATA_MODE);
		index++;
	}

	lcd_unlock();
}

void lcd_write_data_at(pcf8574_lcd_t *lcd, uint8_t row, uint8_t col, const char *label, uint32_t value)
{
	char lcd_text[PCF8574_LCD_COLS + 1u];

	if (NULL == label)
	{
		label = "Dato";
	}

	(void)snprintf(lcd_text, sizeof(lcd_text), "%s:%lu", label, value);
	lcd_write_string_at(lcd, row, col, lcd_text);
}

void lcd_write_hex8_at(pcf8574_lcd_t *lcd, uint8_t row, uint8_t col, const char *label, uint8_t value)
{
	char lcd_text[PCF8574_LCD_COLS + 1u];

	if (NULL == label)
	{
		label = "Dato";
	}

	(void)snprintf(lcd_text, sizeof(lcd_text), "%s:0x%02X", label, value);
	lcd_write_string_at(lcd, row, col, lcd_text);
}

/********************** end of file ******************************************/
