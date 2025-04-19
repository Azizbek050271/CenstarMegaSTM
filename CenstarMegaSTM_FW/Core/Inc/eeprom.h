/* eeprom.h - Заголовочный файл для работы с EEPROM 24C256 */

#ifndef EEPROM_H
#define EEPROM_H

#include "stm32f4xx_hal.h"
#include "fsm.h"
#include <stdbool.h>
#include "FreeRTOS.h"
#include "queue.h"

// Структура запроса к EEPROM
typedef struct {
    bool isWrite; // true: запись, false: чтение
    uint16_t memAddr;
    union {
        struct { // Для записи
            uint32_t liters;
            uint32_t price;
            FSMState state;
            FuelMode mode;
            bool modeSelected;
        } transaction;
        uint16_t price; // Для записи цены
    } data;
    // Для чтения транзакции
    uint32_t* litersOut;
    uint32_t* priceOut;
    FSMState* stateOut;
    FuelMode* modeOut;
    bool* modeSelectedOut;
    // Для чтения цены
    uint16_t* priceOutSimple;
} EEPROMRequest;

// Инициализация и обработка запросов (для задачи FreeRTOS)
void handleEEPROMRequest(EEPROMRequest* req);

// Функции для вызова из других модулей
void writePriceToEEPROM(uint16_t price);
uint16_t readPriceFromEEPROM(void);
void saveTransactionState(uint32_t liters, uint32_t price, FSMState state, FuelMode mode, bool modeSelected);
bool restoreTransactionState(uint32_t* liters, uint32_t* price, FSMState* state, FuelMode* mode, bool* modeSelected);

#endif /* EEPROM_H */
