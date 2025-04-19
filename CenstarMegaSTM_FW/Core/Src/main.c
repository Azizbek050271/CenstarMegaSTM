/* main.c - Главный файл проекта CenstarMegaSTM_FW для STM32F407G-DISC1
 * Инициализация FreeRTOS задач и периферии
 */

#include "main.h"
#include "cmsis_os.h"
#include "config.h"
#include "fsm.h"
#include "keypad.h"
#include "oled.h"
#include "rs422.h"
#include "eeprom.h"
#include <stdio.h>

// Дескрипторы периферии (сгенерированы CubeMX)
I2C_HandleTypeDef hi2c1;
UART_HandleTypeDef huart2;  // Для RS-422
UART_HandleTypeDef huart3;  // Для логов
IWDG_HandleTypeDef hiwdg;
TIM_HandleTypeDef htim2;

// Очереди FreeRTOS
QueueHandle_t keypadQueue;    // Очередь для клавиш
QueueHandle_t oledQueue;      // Очередь для сообщений OLED
QueueHandle_t rs422TxQueue;   // Очередь для отправки команд RS-422
QueueHandle_t rs422RxQueue;   // Очередь для приёма ответов RS-422
QueueHandle_t eepromQueue;    // Очередь для операций с EEPROM

// Контекст FSM
FSMContext fsmContext;

// Прототипы задач FreeRTOS
void StartFSMTask(void *argument);
void StartKeypadTask(void *argument);
void StartRS422Task(void *argument);
void StartOLEDTask(void *argument);
void StartEEPROMTask(void *argument);
void StartWatchdogTask(void *argument);

// Прототипы функций инициализации
void SystemClock_Config(void);
void MX_GPIO_Init(void);
void MX_DMA_Init(void);
void MX_I2C1_Init(void);
void MX_USART2_UART_Init(void);
void MX_USART3_UART_Init(void);
void MX_IWDG_Init(void);
void MX_TIM2_Init(void);

// Глобальные переменные
static volatile uint32_t tim2_counter = 0; // Счётчик для замены millis()

int main(void)
{
    // Инициализация HAL
    HAL_Init();

    // Настройка системного тактирования
    SystemClock_Config();

    // Инициализация периферии
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_I2C1_Init();
    MX_USART2_UART_Init();
    MX_USART3_UART_Init();
    MX_IWDG_Init();
    MX_TIM2_Init();

    // Запуск TIM2 для отсчёта времени
    HAL_TIM_Base_Start_IT(&htim2);

    // Создание очередей FreeRTOS
    keypadQueue = xQueueCreate(10, sizeof(char));              // Очередь для клавиш
    oledQueue = xQueueCreate(5, 128 * sizeof(char));          // Очередь для сообщений OLED
    rs422TxQueue = xQueueCreate(10, sizeof(RS422Command));    // Очередь для команд RS-422
    rs422RxQueue = xQueueCreate(10, 32 * sizeof(uint8_t));    // Очередь для ответов RS-422
    eepromQueue = xQueueCreate(5, sizeof(EEPROMRequest));     // Очередь для операций с EEPROM

    // Проверка создания очередей
    if (keypadQueue == NULL || oledQueue == NULL || rs422TxQueue == NULL ||
        rs422RxQueue == NULL || eepromQueue == NULL) {
        Error_Handler();
    }

    // Создание задач FreeRTOS
    xTaskCreate(StartFSMTask, "FSM", 512, NULL, 3, NULL);         // Задача FSM
    xTaskCreate(StartKeypadTask, "Keypad", 256, NULL, 4, NULL);   // Задача клавиатуры
    xTaskCreate(StartRS422Task, "RS422", 512, NULL, 4, NULL);     // Задача RS-422
    xTaskCreate(StartOLEDTask, "OLED", 256, NULL, 2, NULL);       // Задача OLED
    xTaskCreate(StartEEPROMTask, "EEPROM", 256, NULL, 2, NULL);   // Задача EEPROM
    xTaskCreate(StartWatchdogTask, "Watchdog", 128, NULL, 5, NULL); // Задача Watchdog

    // Запуск планировщика FreeRTOS
    osKernelStart();

    // Этот код никогда не будет достигнут
    while (1) {}
}

// Задача FSM
void StartFSMTask(void *argument)
{
    initFSM(&fsmContext);
    for (;;) {
        updateFSM(&fsmContext);
        char key;
        if (xQueueReceive(keypadQueue, &key, 0) == pdTRUE) {
            processKeyFSM(&fsmContext, key);
        }
        osDelay(10); // Периодичность 10 мс
    }
}

// Задача клавиатуры
void StartKeypadTask(void *argument)
{
    for (;;) {
        char key = getKeypadKey();
        if (key) {
            xQueueSend(keypadQueue, &key, portMAX_DELAY);
        }
        osDelay(20); // Периодичность 20 мс (с учётом антидребезга)
    }
}

// Задача RS-422
void StartRS422Task(void *argument)
{
    initRS422();
    RS422Command cmd;
    for (;;) {
        if (xQueueReceive(rs422TxQueue, &cmd, portMAX_DELAY) == pdTRUE) {
            sendRS422Command(&cmd);
        }
    }
}

// Задача OLED
void StartOLEDTask(void *argument)
{
    initOLED();
    char msg[128];
    for (;;) {
        if (xQueueReceive(oledQueue, msg, portMAX_DELAY) == pdTRUE) {
            displayMessage(msg);
        }
    }
}

// Задача EEPROM
void StartEEPROMTask(void *argument)
{
    EEPROMRequest req;
    for (;;) {
        if (xQueueReceive(eepromQueue, &req, portMAX_DELAY) == pdTRUE) {
            handleEEPROMRequest(&req);
        }
    }
}

// Задача Watchdog
void StartWatchdogTask(void *argument)
{
    for (;;) {
        HAL_IWDG_Refresh(&hiwdg); // Сброс Watchdog каждые 500 мс
        osDelay(500);
    }
}

// Обработчик прерывания TIM2 (замена millis())
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2) {
        tim2_counter++; // Инкремент счётчика каждую миллисекунду
    }
}

// Функция для получения текущего времени (замена millis())
uint32_t getCurrentMillis(void)
{
    return tim2_counter;
}

// Функции инициализации (сгенерированы CubeMX, оставлены без изменений)
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI|RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.LSIState = RCC_LSI_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 8;
    RCC_OscInitStruct.PLL.PLLN = 336;
    RCC_OscInitStruct.PLL.PLLQ = 7;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) Error_Handler();

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                                 |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK) Error_Handler();
}

void MX_I2C1_Init(void)
{
    hi2c1.Instance = I2C1;
    hi2c1.Init.ClockSpeed = 400000;
    hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
    hi2c1.Init.OwnAddress1 = 0;
    hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2 = 0;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&hi2c1) != HAL_OK) Error_Handler();
}

void MX_USART2_UART_Init(void)
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

void MX_USART3_UART_Init(void)
{
    huart3.Instance = USART3;
    huart3.Init.BaudRate = 9600;
    huart3.Init.WordLength = UART_WORDLENGTH_8B;
    huart3.Init.StopBits = UART_STOPBITS_1;
    huart3.Init.Parity = UART_PARITY_NONE;
    huart3.Init.Mode = UART_MODE_TX_RX;
    huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart3.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart3) != HAL_OK) Error_Handler();
}

void MX_IWDG_Init(void)
{
    hiwdg.Instance = IWDG;
    hiwdg.Init.Prescaler = IWDG_PRESCALER_32;
    hiwdg.Init.Reload = 4095;
    if (HAL_IWDG_Init(&hiwdg) != HAL_OK) Error_Handler();
}

void MX_TIM2_Init(void)
{
    TIM_ClockConfigTypeDef sClockSourceConfig = {0};
    TIM_MasterConfigTypeDef sMasterConfig = {0};

    htim2.Instance = TIM2;
    htim2.Init.Prescaler = 8399;
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period = 4294967295;
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    if (HAL_TIM_Base_Init(&htim2) != HAL_OK) Error_Handler();

    sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
    if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK) Error_Handler();

    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK) Error_Handler();
}

void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOH_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();

    // Клавиатура: строки (выходы)
    GPIO_InitStruct.Pin = ROW1_PIN | ROW2_PIN | ROW3_PIN | ROW4_PIN | ROW5_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    // Клавиатура: столбцы (входы с подтяжкой)
    GPIO_InitStruct.Pin = COL1_PIN | COL2_PIN | COL3_PIN | COL4_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    // Опционально: LED для отладки (можно закомментировать, если не нужны)
    /*
    GPIO_InitStruct.Pin = GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
    */
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) {}
}
