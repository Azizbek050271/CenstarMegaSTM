/* ssd1306.c  –  минимальная реализация */

#include "ssd1306.h"
#include <string.h>

static uint8_t Buffer[SSD1306_WIDTH * SSD1306_HEIGHT / 8];
static uint8_t CurrentX = 0;
static uint8_t CurrentY = 0;

static HAL_StatusTypeDef ssd1306_WriteCommand(uint8_t cmd)
{
    uint8_t d[2] = {0x00, cmd};
    return HAL_I2C_Master_Transmit(&hi2c1, SSD1306_I2C_ADDR, d, 2, HAL_MAX_DELAY);
}

void ssd1306_Init(void)
{
    HAL_Delay(100);
    ssd1306_WriteCommand(0xAE); // Display off
    ssd1306_WriteCommand(0x20); // Memory addressing mode = horizontal
    ssd1306_WriteCommand(0x00);

    ssd1306_WriteCommand(0xB0); // Page Start
    ssd1306_WriteCommand(0xC8); // COM Output Scan Direction
    ssd1306_WriteCommand(0x00); // Low column
    ssd1306_WriteCommand(0x10); // High column
    ssd1306_WriteCommand(0x40); // Start line address
    ssd1306_WriteCommand(0x81); // Contrast
    ssd1306_WriteCommand(0x7F);

    ssd1306_WriteCommand(0xA1); // Segment remap
    ssd1306_WriteCommand(0xA6); // Normal display
    ssd1306_WriteCommand(0xA8); // Multiplex ratio
    ssd1306_WriteCommand(0x3F);

    ssd1306_WriteCommand(0xA4); // Output follows RAM
    ssd1306_WriteCommand(0xD3); // Display offset
    ssd1306_WriteCommand(0x00);

    ssd1306_WriteCommand(0xD5); // Display clock divide
    ssd1306_WriteCommand(0xF0);
    ssd1306_WriteCommand(0xD9); // Pre‑charge
    ssd1306_WriteCommand(0x22);
    ssd1306_WriteCommand(0xDA); // COM Pins
    ssd1306_WriteCommand(0x12);
    ssd1306_WriteCommand(0xDB); // VCOM detect
    ssd1306_WriteCommand(0x20);
    ssd1306_WriteCommand(0x8D); // Charge pump
    ssd1306_WriteCommand(0x14);

    ssd1306_WriteCommand(0xAF); // Display ON

    ssd1306_Fill(SSD1306_COLOR_BLACK);
    ssd1306_UpdateScreen();
}

void ssd1306_UpdateScreen(void)
{
    for (uint8_t page = 0; page < 8; page++)
    {
        ssd1306_WriteCommand(0xB0 + page);
        ssd1306_WriteCommand(0x00);
        ssd1306_WriteCommand(0x10);
        HAL_I2C_Mem_Write(&hi2c1, SSD1306_I2C_ADDR, 0x40,
                          I2C_MEMADD_SIZE_8BIT,
                          &Buffer[SSD1306_WIDTH * page],
                          SSD1306_WIDTH, HAL_MAX_DELAY);
    }
}

void ssd1306_Fill(SSD1306_COLOR color)
{
    memset(Buffer, color ? 0xFF : 0x00, sizeof(Buffer));
}

void ssd1306_SetCursor(uint8_t x, uint8_t y)
{
    CurrentX = x;
    CurrentY = y;
}

/* --- очень упрощённый вывод символов шрифтом 5x7 --- */
static const uint8_t Font5x7[] = {
#include "font5x7.inc"           // вставь сюда массив или реализуй позже
};

void ssd1306_WriteChar(char ch, SSD1306_COLOR color)
{
    if (ch < 32 || ch > 126) ch = '?';
    const uint8_t* glyph = &Font5x7[(ch - 32) * 5];
    for (uint8_t i = 0; i < 5; i++)
    {
        uint8_t line = glyph[i];
        for (uint8_t j = 0; j < 8; j++)
        {
            uint32_t idx = CurrentX + (CurrentY / 8) * SSD1306_WIDTH;
            if (line & 0x01)
                Buffer[idx] |= (1 << (CurrentY % 8));
            else
                Buffer[idx] &= ~(1 << (CurrentY % 8));
            line >>= 1;
            CurrentY++;
        }
        CurrentX++;
        CurrentY -= 8;
    }
    CurrentX++; // пробел
}

void ssd1306_WriteString(const char* str, SSD1306_COLOR color)
{
    while (*str)
    {
        ssd1306_WriteChar(*str++, color);
    }
}
