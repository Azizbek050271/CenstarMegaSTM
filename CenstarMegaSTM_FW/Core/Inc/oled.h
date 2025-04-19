/* oled.h - Заголовочный файл для управления OLED-дисплеем SSD1306 */

#ifndef OLED_H
#define OLED_H

#include "stm32f4xx_hal.h"
#include <stdbool.h>
#include "FreeRTOS.h"
#include "queue.h"

#define SSD1306_WIDTH  128
#define SSD1306_HEIGHT 64

typedef enum {
    SSD1306_COLOR_BLACK = 0x00,
    SSD1306_COLOR_WHITE = 0x01
} SSD1306_COLOR;

// Инициализация дисплея
void initOLED(void);

// Отображение сообщения
bool displayMessage(const char* msg);

// Низкоуровневые функции (взяты из вашего тестового кода)
void ssd1306_UpdateScreen(void);
void ssd1306_Fill(SSD1306_COLOR color);
void ssd1306_SetCursor(uint8_t x, uint8_t y);
void ssd1306_WriteChar(char ch, SSD1306_COLOR color);
void ssd1306_WriteString(const char* str, SSD1306_COLOR color);

#endif /* OLED_H */
