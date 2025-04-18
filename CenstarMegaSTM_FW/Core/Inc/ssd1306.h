/* ssd1306.h  –  минимальный HAL‑вариант для STM32 I2C */

#ifndef __SSD1306_H__
#define __SSD1306_H__

#include "stm32f4xx_hal.h"

/* ==== Настройки дисплея ==== */
#define SSD1306_WIDTH      128
#define SSD1306_HEIGHT      64
#define SSD1306_I2C_ADDR   (0x3C << 1)   // 0x3C на шине -> <<1 для HAL
#define SSD1306_USE_I2C

extern I2C_HandleTypeDef hi2c1;          // описан CubeMX

typedef enum {
    SSD1306_COLOR_BLACK = 0x00, // чёрный пиксель
    SSD1306_COLOR_WHITE = 0x01  // белый  пиксель
} SSD1306_COLOR;

/* Публичные функции */
void     ssd1306_Init(void);
void     ssd1306_UpdateScreen(void);
void     ssd1306_Fill(SSD1306_COLOR color);
void     ssd1306_SetCursor(uint8_t x, uint8_t y);
void     ssd1306_WriteChar(char ch, SSD1306_COLOR color);
void     ssd1306_WriteString(const char* str, SSD1306_COLOR color);

#endif
