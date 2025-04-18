#include "ssd1306.h"
#include <string.h>

/* === внутренний буфер 1 КБ === */
static uint8_t Buffer[SSD1306_WIDTH * SSD1306_HEIGHT / 8];
static uint8_t CurrentX, CurrentY;

/* === маленький шрифт 5×7 (ASCII 32‑126) === */
static const uint8_t Font5x7[] = {
  #include "font5x7.inc"        /* файл приведён ниже */
};

/* ---- низкоуровневые функции ---- */
static HAL_StatusTypeDef CMD(uint8_t c)
{
    uint8_t d[2] = {0x00, c};
    return HAL_I2C_Master_Transmit(&hi2c1, SSD1306_I2C_ADDR, d, 2, HAL_MAX_DELAY);
}

/* ---- публичные ---- */
void ssd1306_Init(void)
{
    HAL_Delay(100);
    CMD(0xAE); CMD(0x20); CMD(0x00); CMD(0xB0); CMD(0xC8);
    CMD(0x00); CMD(0x10); CMD(0x40); CMD(0x81); CMD(0x7F);
    CMD(0xA1); CMD(0xA6); CMD(0xA8); CMD(0x3F); CMD(0xA4);
    CMD(0xD3); CMD(0x00); CMD(0xD5); CMD(0xF0); CMD(0xD9);
    CMD(0x22); CMD(0xDA); CMD(0x12); CMD(0xDB); CMD(0x20);
    CMD(0x8D); CMD(0x14); CMD(0xAF);

    ssd1306_Fill(SSD1306_COLOR_BLACK);
    ssd1306_UpdateScreen();
}

void ssd1306_UpdateScreen(void)
{
    for (uint8_t page = 0; page < 8; page++)
    {
        CMD(0xB0 + page); CMD(0x00); CMD(0x10);
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

void ssd1306_WriteChar(char ch, SSD1306_COLOR color)
{
    if (ch < 32 || ch > 126) ch = '?';
    const uint8_t* glyph = &Font5x7[(ch - 32) * 5];

    for (uint8_t col = 0; col < 5; col++)
    {
        uint8_t line = glyph[col];
        for (uint8_t row = 0; row < 7; row++)
        {
            uint32_t idx = CurrentX + ((CurrentY + row) / 8) * SSD1306_WIDTH;
            uint8_t  bit = 1 << ((CurrentY + row) % 8);

            if (line & 0x01)
                Buffer[idx] |=  bit;
            else
                Buffer[idx] &= ~bit;
            line >>= 1;
        }
        CurrentX++;
    }
    CurrentX++; /* один столбец пробела */
}

void ssd1306_WriteString(const char* str, SSD1306_COLOR color)
{
    while (*str)
        ssd1306_WriteChar(*str++, color);
}

/* маленький тест: вывод "Hello OLED!" */
void ssd1306_Test(void)
{
    ssd1306_Fill(SSD1306_COLOR_BLACK);
    ssd1306_SetCursor(0, 0);
    ssd1306_WriteString("Hello OLED!", SSD1306_COLOR_WHITE);
    ssd1306_UpdateScreen();
}
