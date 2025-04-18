#ifndef __SSD1306_H__
#define __SSD1306_H__

#include "stm32f4xx_hal.h"

#define SSD1306_WIDTH     128
#define SSD1306_HEIGHT     64
#define SSD1306_I2C_ADDR  (0x3C << 1)   /* 0x3C на шине → <<1 для HAL   */

extern I2C_HandleTypeDef hi2c1;

typedef enum {
    SSD1306_COLOR_BLACK = 0x00,
    SSD1306_COLOR_WHITE = 0x01
} SSD1306_COLOR;

/* публичные */
void ssd1306_Init(void);
void ssd1306_UpdateScreen(void);
void ssd1306_Fill(SSD1306_COLOR color);
void ssd1306_SetCursor(uint8_t x, uint8_t y);
void ssd1306_WriteChar(char ch, SSD1306_COLOR color);
void ssd1306_WriteString(const char* str, SSD1306_COLOR color);

/* небольшая проверка: выведет Hello OLED! */
void ssd1306_Test(void);

#endif
