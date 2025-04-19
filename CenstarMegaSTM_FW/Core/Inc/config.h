/* config.h - Конфигурация проекта CenstarMegaSTM_FW для STM32F407G-DISC1 */

#ifndef CONFIG_H
#define CONFIG_H

#include "stm32f4xx_hal.h"

// Параметры OLED-дисплея
#define SCREEN_WIDTH 128        // Ширина экрана в пикселях
#define SCREEN_HEIGHT 64        // Высота экрана в пикселях
#define OLED_I2C_ADDR (0x3C << 1) // I2C-адрес дисплея (0x3C на шине, сдвинутый для HAL)

// Параметры клавиатуры
#define KEYPAD_ROW_COUNT 5      // Количество строк клавиатуры
#define KEYPAD_COL_COUNT 4      // Количество столбцов клавиатуры
// Пины строк (выходы)
#define ROW1_PORT GPIOC
#define ROW1_PIN  GPIO_PIN_0
#define ROW2_PORT GPIOC
#define ROW2_PIN  GPIO_PIN_1
#define ROW3_PORT GPIOC
#define ROW3_PIN  GPIO_PIN_2
#define ROW4_PORT GPIOC
#define ROW4_PIN  GPIO_PIN_3
#define ROW5_PORT GPIOC
#define ROW5_PIN  GPIO_PIN_4
// Пины столбцов (входы с подтяжкой)
#define COL1_PORT GPIOB
#define COL1_PIN  GPIO_PIN_0
#define COL2_PORT GPIOB
#define COL2_PIN  GPIO_PIN_1
#define COL3_PORT GPIOB
#define COL3_PIN  GPIO_PIN_11
#define COL4_PORT GPIOB
#define COL4_PIN  GPIO_PIN_12
#define KEY_DEBOUNCE_MS 15      // Антидребезг клавиш (мс)

// Параметры интерфейса RS-422
#define RS422_BAUD_RATE 9600    // Скорость передачи данных (бод)

// Параметры EEPROM (24C256)
#define EEPROM_I2C_ADDR (0x50 << 1) // I2C-адрес (0x50 на шине, сдвинутый для HAL)
#define EEPROM_PAGE_SIZE 64     // Размер страницы в байтах
#define EEPROM_SIZE 32768       // 32 КБ

// Таймауты и задержки
#define RESPONSE_TIMEOUT 3000   // Максимальное время ожидания ответа ТРК (мс)
#define INTERBYTE_TIMEOUT 3     // Таймаут между байтами в ответе (мс)
#define DELAY_AFTER_RESPONSE 3  // Задержка после получения ответа (мс)
#define DISPLAY_WELCOME_DURATION 500 // Длительность отображения приветствия (мс)
#define EDIT_TIMEOUT 10000      // Таймаут редактирования цены (мс)
#define VIEW_TIMEOUT 2000       // Таймаут просмотра цены (мс)
#define TRANSITION_TIMEOUT 2000 // Таймаут переходных состояний (мс)

// Параметры ввода цены
#define PRICE_FORMAT_LENGTH 7   // Максимальная длина ввода цены (символы)
#define PRICE_MIN 0             // Минимальная цена
#define PRICE_MAX 99999         // Максимальная цена

// Длины ответов протокола
#define STATUS_RESPONSE_LENGTH 7            // Длина ответа на команду статуса
#define MONITOR_RESPONSE_LENGTH 15          // Длина ответа на команды L и R
#define TRANSACTION_END_RESPONSE_LENGTH 27  // Длина ответа на команду T
#define TOTAL_COUNTER_RESPONSE_LENGTH 16    // Длина ответа на команду C

// Прочие параметры
#define MAX_ERROR_COUNT 5       // Максимальное число ошибок перед TRK Error
#define NOZZLE_COUNT 6          // Максимальное число рукавов
#define POST_ADDRESS 1          // Адрес поста (1-32)

// Параметры логирования
#define LOG_LEVEL_DEBUG 0       // Уровень отладочных сообщений
#define LOG_LEVEL_ERROR 1       // Уровень сообщений об ошибках
#define LOG_LEVEL LOG_LEVEL_DEBUG // Текущий уровень логирования

// Параметры кадров протокола
#define MAX_FRAME_PAYLOAD 16    // Максимальная длина полезной нагрузки кадра

#endif /* CONFIG_H */
