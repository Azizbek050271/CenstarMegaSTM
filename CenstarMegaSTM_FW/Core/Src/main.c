/* main.c – UART‑I2C тест с OLED вспышкой и I2C‑сканером */

#include "main.h"
#include <stdio.h>
#include <string.h>
#include "ssd1306.h"

/* -------- CubeMX global handles -------- */
I2C_HandleTypeDef hi2c1;
I2S_HandleTypeDef hi2s3;
UART_HandleTypeDef huart2;
/* --------------------------------------- */

/* prototypes (CubeMX) */
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

  HAL_UART_Transmit(&huart2,
        (uint8_t*)"Hello STM32\r\n", 13, HAL_MAX_DELAY);

  /* --- Инициализация OLED --- */
  ssd1306_Init();

  /* Вспышка белым на 1 секунду */
  ssd1306_Fill(SSD1306_COLOR_WHITE);
  ssd1306_UpdateScreen();
  HAL_Delay(1000);

  /* Показ тестовой строки */
  ssd1306_Test();                 /* ← новая функция выводит "Hello OLED!" */

  uint32_t pass = 0;
  char msg[32];

  while (1)
  {
      sprintf(msg, "\r\nScan #%lu\r\n", pass++);
      HAL_UART_Transmit(&huart2,(uint8_t*)msg,strlen(msg),HAL_MAX_DELAY);

      for (uint8_t addr = 1; addr < 128; addr++)
      {
          if (HAL_I2C_IsDeviceReady(&hi2c1, addr << 1, 1, 10) == HAL_OK)
          {
              sprintf(msg, "Found 0x%02X\r\n", addr);
              HAL_UART_Transmit(&huart2,(uint8_t*)msg,strlen(msg),HAL_MAX_DELAY);
          }
      }
      HAL_UART_Transmit(&huart2,(uint8_t*)"Scan done\r\n",11,HAL_MAX_DELAY);
      HAL_Delay(2000);
  }
}

/* ===== CubeMX‑generated functions (не менять) =================== */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState       = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState   = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM       = 8;
  RCC_OscInitStruct.PLL.PLLN       = 336;
  RCC_OscInitStruct.PLL.PLLP       = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ       = 7;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) Error_Handler();

  RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                   | RCC_CLOCKTYPE_PCLK1| RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5)!=HAL_OK)
      Error_Handler();
}

static void MX_I2C1_Init(void)
{
  hi2c1.Instance             = I2C1;
  hi2c1.Init.ClockSpeed      = 100000;
  hi2c1.Init.DutyCycle       = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1     = 0;
  hi2c1.Init.AddressingMode  = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2     = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode   = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK) Error_Handler();
}

static void MX_I2S3_Init(void)
{
  hi2s3.Instance          = SPI3;
  hi2s3.Init.Mode         = I2S_MODE_MASTER_TX;
  hi2s3.Init.Standard     = I2S_STANDARD_PHILIPS;
  hi2s3.Init.DataFormat   = I2S_DATAFORMAT_16B;
  hi2s3.Init.MCLKOutput   = I2S_MCLKOUTPUT_ENABLE;
  hi2s3.Init.AudioFreq    = I2S_AUDIOFREQ_96K;
  hi2s3.Init.CPOL         = I2S_CPOL_LOW;
  hi2s3.Init.ClockSource  = I2S_CLOCK_PLL;
  hi2s3.Init.FullDuplexMode=I2S_FULLDUPLEXMODE_DISABLE;
  if (HAL_I2S_Init(&hi2s3) != HAL_OK) Error_Handler();
}

static void MX_USART2_UART_Init(void)
{
  huart2.Instance          = USART2;
  huart2.Init.BaudRate     = 9600;
  huart2.Init.WordLength   = UART_WORDLENGTH_8B;
  huart2.Init.StopBits     = UART_STOPBITS_1;
  huart2.Init.Parity       = UART_PARITY_NONE;
  huart2.Init.Mode         = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
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

  /* Сгенерированные CubeMX настройки пинов Discovery */
  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14|
                           GPIO_PIN_15, GPIO_PIN_RESET);

  GPIO_InitStruct.Pin   = GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
}

void Error_Handler(void)
{
  __disable_irq();
  while (1) { }
}
