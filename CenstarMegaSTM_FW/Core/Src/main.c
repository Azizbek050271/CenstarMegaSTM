/* STM32F407 – CenstarMegaSTM_FW
 * OLED SSD1306, Keypad 5×4, 24C256 EEPROM, UART2 logs
 * Rows PC0–PC4, Cols PB0/PB1/PB11/PB12 (see keypad.c)
 */

#include "main.h"
#include <stdio.h>
#include <string.h>

#include "ssd1306.h"
#include "keypad.h"
#include "eeprom24c256.h"

/* ---- CubeMX global handles ---- */
I2C_HandleTypeDef hi2c1;
I2S_HandleTypeDef hi2s3;
UART_HandleTypeDef huart2;
/* ---------------------------------- */

/* CubeMX prototypes */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_I2S3_Init(void);
static void MX_USART2_UART_Init(void);

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_I2C1_Init();
    MX_I2S3_Init();
    MX_USART2_UART_Init();

    /* --- UART2 greeting --- */
    HAL_UART_Transmit(&huart2, (uint8_t*)"Hello STM32\r\n", 13, HAL_MAX_DELAY);

    /* --- OLED: flash and splash screen --- */
    ssd1306_Init();
    ssd1306_Fill(SSD1306_COLOR_WHITE);
    ssd1306_UpdateScreen();
    HAL_Delay(1000);
    ssd1306_Test(); /* "Hello OLED!" */

    /* === EEPROM test =================================================== */
    {
        /* Check if 24C256 (address 0x50) is accessible */
        HAL_UART_Transmit(&huart2, (uint8_t*)"Checking EEPROM...\r\n", 20, HAL_MAX_DELAY);
        if (HAL_I2C_IsDeviceReady(&hi2c1, EEPROM_I2C_ADDR, 2, 10) != HAL_OK)
        {
            HAL_UART_Transmit(&huart2, (uint8_t*)"EEPROM missing!\r\n", 17, HAL_MAX_DELAY);

            ssd1306_Fill(SSD1306_COLOR_BLACK);
            ssd1306_SetCursor(0, 0);
            ssd1306_WriteString("EEPROM MISSING!", SSD1306_COLOR_WHITE);
            ssd1306_UpdateScreen();
            HAL_Delay(1500);
            ssd1306_Test();
        }
        else
        {
            uint8_t wbuf[] = "Hello EEPROM!";
            uint8_t rbuf[sizeof wbuf] = {0};

            HAL_UART_Transmit(&huart2, (uint8_t*)"Starting EEPROM test...\r\n", 25, HAL_MAX_DELAY);
            if (EEPROM_Write(0x0000, wbuf, sizeof wbuf) == HAL_OK &&
                EEPROM_Read(0x0000, rbuf, sizeof rbuf) == HAL_OK)
            {
                char msg[40];
                sprintf(msg, "EEPROM: %s\r\n", rbuf);
                HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), HAL_MAX_DELAY);

                ssd1306_Fill(SSD1306_COLOR_BLACK);
                ssd1306_SetCursor(0, 0);
                ssd1306_WriteString("EEPROM OK:", SSD1306_COLOR_WHITE);
                ssd1306_SetCursor(0, 12);
                ssd1306_WriteString((char*)rbuf, SSD1306_COLOR_WHITE);
                ssd1306_UpdateScreen();
                HAL_Delay(1500);
                ssd1306_Test();
            }
            else
            {
                HAL_UART_Transmit(&huart2, (uint8_t*)"EEPROM error\r\n", 14, HAL_MAX_DELAY);

                ssd1306_Fill(SSD1306_COLOR_BLACK);
                ssd1306_SetCursor(0, 0);
                ssd1306_WriteString("EEPROM ERROR!", SSD1306_COLOR_WHITE);
                ssd1306_UpdateScreen();
                HAL_Delay(1500);
                ssd1306_Test();
            }
        }
    }
    /* ==================================================================== */

    char buf[32];

    while (1)
    {
        /* ---------- Keypad polling ---------- */
        char k = KEYPAD_Scan();
        if (k)
        {
            sprintf(buf, "Key [%c]\r\n", k);
            HAL_UART_Transmit(&huart2, (uint8_t*)buf, strlen(buf), HAL_MAX_DELAY);

            ssd1306_Fill(SSD1306_COLOR_BLACK);
            ssd1306_SetCursor(0, 0);
            ssd1306_WriteString("Key:", SSD1306_COLOR_WHITE);
            ssd1306_SetCursor(40, 0);
            ssd1306_WriteChar(k, SSD1306_COLOR_WHITE);
            ssd1306_UpdateScreen();
            HAL_Delay(1000);
            ssd1306_Test();
        }

        HAL_Delay(50); /* Main loop ~20 Hz */
    }
}

/* ----- CubeMX functions (leave unchanged) -------- */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 8;
    RCC_OscInitStruct.PLL.PLLN = 336;
    RCC_OscInitStruct.PLL.PLLQ = 7;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) Error_Handler();

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
        Error_Handler();
}

static void MX_I2C1_Init(void)
{
    hi2c1.Instance = I2C1;
    hi2c1.Init.ClockSpeed = 100000;
    hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
    hi2c1.Init.OwnAddress1 = 0;
    hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2 = 0;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&hi2c1) != HAL_OK) Error_Handler();
}

static void MX_I2S3_Init(void)
{
    hi2s3.Instance = SPI3;
    hi2s3.Init.Mode = I2S_MODE_MASTER_TX;
    hi2s3.Init.Standard = I2S_STANDARD_PHILIPS;
    hi2s3.Init.DataFormat = I2S_DATAFORMAT_16B;
    hi2s3.Init.MCLKOutput = I2S_MCLKOUTPUT_ENABLE;
    hi2s3.Init.AudioFreq = I2S_AUDIOFREQ_96K;
    hi2s3.Init.CPOL = I2S_CPOL_LOW;
    hi2s3.Init.ClockSource = I2S_CLOCK_PLL;
    hi2s3.Init.FullDuplexMode = I2S_FULLDUPLEXMODE_DISABLE;
    if (HAL_I2S_Init(&hi2s3) != HAL_OK) Error_Handler();
}

static void MX_USART2_UART_Init(void)
{
    huart2.Instance = USART2;
    huart2.Init.BaudRate = 9600;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart2) != HAL_OK) Error_Handler();
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOH_CLK_ENABLE();

    /* Keypad rows (PC0–PC4) */
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    /* Keypad columns (PB0, PB1, PB11, PB12) */
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_11 | GPIO_PIN_12;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) { }
}
