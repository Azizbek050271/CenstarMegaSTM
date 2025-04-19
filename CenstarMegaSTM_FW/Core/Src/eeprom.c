/* eeprom.c - Реализация работы с EEPROM 24C256 через I2C */

#include "eeprom.h"
#include "config.h"
#include <cmsis_os.h>
#include <stdbool.h>

extern I2C_HandleTypeDef hi2c1;

// Адреса в EEPROM для хранения данных
#define EEPROM_PRICE_ADDR 0
#define EEPROM_LITERS_ADDR 4
#define EEPROM_PRICE_TOTAL_ADDR 8
#define EEPROM_STATE_ADDR 12
#define EEPROM_MODE_ADDR 14
#define EEPROM_MODE_SELECTED_ADDR 15

// Низкоуровневые функции чтения/записи (адаптированы из вашего тестового кода)
static HAL_StatusTypeDef EEPROM_WaitReady(void) {
    uint32_t tick = HAL_GetTick();
    while (HAL_I2C_IsDeviceReady(&hi2c1, EEPROM_I2C_ADDR, 1, 10) != HAL_OK) {
        if (HAL_GetTick() - tick > 25) {
            logMessage(LOG_LEVEL_ERROR, "EEPROM Timeout");
            return HAL_TIMEOUT;
        }
    }
    return HAL_OK;
}

static HAL_StatusTypeDef EEPROM_Write(uint16_t memAddr, const uint8_t* data, uint16_t len) {
    if (HAL_I2C_IsDeviceReady(&hi2c1, EEPROM_I2C_ADDR, 2, 10) != HAL_OK) {
        logMessage(LOG_LEVEL_ERROR, "EEPROM Not Found");
        return HAL_ERROR;
    }

    while (len) {
        uint16_t pageRemain = EEPROM_PAGE_SIZE - (memAddr % EEPROM_PAGE_SIZE);
        uint16_t chunk = (len < pageRemain) ? len : pageRemain;

        logMessage(LOG_LEVEL_DEBUG, "Writing chunk...");
        if (HAL_I2C_Mem_Write(&hi2c1, EEPROM_I2C_ADDR, memAddr, I2C_MEMADD_SIZE_16BIT,
                              (uint8_t*)data, chunk, 100) != HAL_OK) {
            logMessage(LOG_LEVEL_ERROR, "EEPROM Write Error");
            return HAL_ERROR;
        }

        if (EEPROM_WaitReady() != HAL_OK) {
            logMessage(LOG_LEVEL_ERROR, "EEPROM Wait Error");
            return HAL_ERROR;
        }

        memAddr += chunk;
        data += chunk;
        len -= chunk;
    }
    logMessage(LOG_LEVEL_DEBUG, "EEPROM Write OK");
    return HAL_OK;
}

static HAL_StatusTypeDef EEPROM_Read(uint16_t memAddr, uint8_t* data, uint16_t len) {
    if (HAL_I2C_IsDeviceReady(&hi2c1, EEPROM_I2C_ADDR, 2, 10) != HAL_OK) {
        logMessage(LOG_LEVEL_ERROR, "EEPROM Not Found");
        return HAL_ERROR;
    }

    HAL_StatusTypeDef status = HAL_I2C_Mem_Read(&hi2c1, EEPROM_I2C_ADDR, memAddr,
                                                I2C_MEMADD_SIZE_16BIT, data, len, 100);
    if (status != HAL_OK) {
        logMessage(LOG_LEVEL_ERROR, "EEPROM Read Error");
        return HAL_ERROR;
    }
    logMessage(LOG_LEVEL_DEBUG, "EEPROM Read OK");
    return HAL_OK;
}

// Обработчик запросов для задачи FreeRTOS
void handleEEPROMRequest(EEPROMRequest* req) {
    if (req->isWrite) {
        if (req->memAddr == EEPROM_PRICE_ADDR) {
            // Запись цены
            uint8_t buffer[2];
            buffer[0] = req->data.price & 0xFF;
            buffer[1] = (req->data.price >> 8) & 0xFF;
            EEPROM_Write(EEPROM_PRICE_ADDR, buffer, 2);
        } else {
            // Запись транзакции
            uint8_t buffer[16];
            // Liters
            buffer[0] = req->data.transaction.liters & 0xFF;
            buffer[1] = (req->data.transaction.liters >> 8) & 0xFF;
            buffer[2] = (req->data.transaction.liters >> 16) & 0xFF;
            buffer[3] = (req->data.transaction.liters >> 24) & 0xFF;
            // Price
            buffer[4] = req->data.transaction.price & 0xFF;
            buffer[5] = (req->data.transaction.price >> 8) & 0xFF;
            buffer[6] = (req->data.transaction.price >> 16) & 0xFF;
            buffer[7] = (req->data.transaction.price >> 24) & 0xFF;
            // State
            buffer[8] = (uint8_t)req->data.transaction.state;
            // Mode
            buffer[9] = (uint8_t)req->data.transaction.mode;
            // Mode Selected
            buffer[10] = (uint8_t)req->data.transaction.modeSelected;
            EEPROM_Write(EEPROM_LITERS_ADDR, buffer, 11);
        }
    } else {
        if (req->memAddr == EEPROM_PRICE_ADDR) {
            // Чтение цены
            uint8_t buffer[2];
            if (EEPROM_Read(EEPROM_PRICE_ADDR, buffer, 2) == HAL_OK) {
                *req->priceOutSimple = (buffer[1] << 8) | buffer[0];
            }
        } else {
            // Чтение транзакции
            uint8_t buffer[16];
            if (EEPROM_Read(EEPROM_LITERS_ADDR, buffer, 11) == HAL_OK) {
                *req->litersOut = (buffer[3] << 24) | (buffer[2] << 16) | (buffer[1] << 8) | buffer[0];
                *req->priceOut = (buffer[7] << 24) | (buffer[6] << 16) | (buffer[5] << 8) | buffer[4];
                *req->stateOut = (FSMState)buffer[8];
                *req->modeOut = (FuelMode)buffer[9];
                *req->modeSelectedOut = (bool)buffer[10];
            }
        }
    }
}

// Функции для вызова из других модулей
void writePriceToEEPROM(uint16_t price) {
    extern QueueHandle_t eepromQueue;
    EEPROMRequest req = {
        .isWrite = true,
        .memAddr = EEPROM_PRICE_ADDR,
        .data.price = price
    };
    xQueueSend(eepromQueue, &req, portMAX_DELAY);
}

uint16_t readPriceFromEEPROM(void) {
    extern QueueHandle_t eepromQueue;
    uint16_t price = 0;
    EEPROMRequest req = {
        .isWrite = false,
        .memAddr = EEPROM_PRICE_ADDR,
        .priceOutSimple = &price
    };
    xQueueSend(eepromQueue, &req, portMAX_DELAY);
    // Ждём выполнения операции (блокирующий вызов)
    while (uxQueueMessagesWaiting(eepromQueue) > 0) {
        osDelay(1);
    }
    return price;
}

void saveTransactionState(uint32_t liters, uint32_t price, FSMState state, FuelMode mode, bool modeSelected) {
    extern QueueHandle_t eepromQueue;
    EEPROMRequest req = {
        .isWrite = true,
        .memAddr = EEPROM_LITERS_ADDR,
        .data.transaction.liters = liters,
        .data.transaction.price = price,
        .data.transaction.state = state,
        .data.transaction.mode = mode,
        .data.transaction.modeSelected = modeSelected
    };
    xQueueSend(eepromQueue, &req, portMAX_DELAY);
}

bool restoreTransactionState(uint32_t* liters, uint32_t* price, FSMState* state, FuelMode* mode, bool* modeSelected) {
    extern QueueHandle_t eepromQueue;
    EEPROMRequest req = {
        .isWrite = false,
        .memAddr = EEPROM_LITERS_ADDR,
        .litersOut = liters,
        .priceOut = price,
        .stateOut = state,
        .modeOut = mode,
        .modeSelectedOut = modeSelected
    };
    xQueueSend(eepromQueue, &req, portMAX_DELAY);
    // Ждём выполнения операции (блокирующий вызов)
    while (uxQueueMessagesWaiting(eepromQueue) > 0) {
        osDelay(1);
    }
    return (*liters != 0xFFFFFFFF && *price != 0xFFFFFFFF && *(uint8_t*)state != 0xFF);
}
