/* fsm.c - Реализация конечного автомата (FSM) для управления топливным насосом */

#include "fsm.h"
#include "config.h"
#include "eeprom.h"
#include "oled.h"
#include "rs422.h"
#include "crc.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <cmsis_os.h>

static SemaphoreHandle_t logMutex; // Мьютекс для синхронизации логов

// Вспомогательные функции форматирования
static void formatLiters(uint32_t dl, char* dst, size_t dstLen) {
    uint32_t intPart = dl / 100;
    uint32_t fracPart = dl % 100;
    snprintf(dst, dstLen, "%lu.%02lu", (unsigned long)intPart, (unsigned long)fracPart);
}

static void displayFuelMode(FuelMode mode) {
    char msg[32];
    switch (mode) {
        case FUEL_BY_VOLUME:    snprintf(msg, sizeof(msg), "Mode: Volume");     break;
        case FUEL_BY_PRICE:     snprintf(msg, sizeof(msg), "Mode: Price");      break;
        case FUEL_BY_FULL_TANK: snprintf(msg, sizeof(msg), "Mode: Full Tank");  break;
    }
    displayMessage(msg);
}

static void displayTransaction(uint32_t liters, uint32_t price, const char* status, bool priceScaled) {
    char litersBuf[12];
    formatLiters(liters, litersBuf, sizeof(litersBuf));
    char displayStr[48];
    uint32_t displayPrice = priceScaled ? price * 10 : price;
    snprintf(displayStr, sizeof(displayStr), "%s\nL: %s\nP: %lu", status, litersBuf, (unsigned long)displayPrice);
    displayMessage(displayStr);
}

// Обработка ответов ТРК
static bool handleResponse(uint8_t* buffer, int length, int expected, FSMContext* ctx) {
    if (length >= expected) {
        ctx->waitingForResponse = false;
        ctx->errorCount = 0;
        return true;
    }
    ctx->waitingForResponse = false;
    ctx->errorCount++;
    if (ctx->errorCount >= MAX_ERROR_COUNT) {
        ctx->state = FSM_STATE_ERROR;
        ctx->stateEntryTime = getCurrentMillis();
        displayMessage("Pump Error");
    }
    return false;
}

// Таблица статусов ТРК
struct StatusAction {
    char code[2];
    FSMState nextState;
    bool resetErrorCount;
    const char* warning;
};

static const struct StatusAction statusActions[] = {
    {{'1', '0'}, FSM_STATE_IDLE, true, NULL},
    {{'2', '1'}, FSM_STATE_IDLE, true, "Nozzle up! Hang up"},
    {{'3', '1'}, FSM_STATE_TRANSACTION, false, NULL},
    {{'4', '1'}, FSM_STATE_TRANSACTION, false, NULL},
    {{'6', '1'}, FSM_STATE_TRANSACTION, false, NULL},
    {{'7', '1'}, FSM_STATE_TRANSACTION_PAUSED, false, NULL},
    {{'8', '1'}, FSM_STATE_TRANSACTION_END, true, NULL},
    {{'9', '0'}, FSM_STATE_IDLE, true, NULL}
};

static bool isValidStatus(uint8_t* buffer) {
    for (size_t i = 0; i < sizeof(statusActions) / sizeof(statusActions[0]); i++) {
        if (buffer[4] == statusActions[i].code[0] && buffer[5] == statusActions[i].code[1]) {
            return true;
        }
    }
    return false;
}

// Обновление состояний FSM
static void updateCheckStatus(FSMContext* ctx) {
    unsigned long currentMillis = getCurrentMillis();
    static unsigned long lastResponseTime = 0;
    static unsigned long nozzleUpStartTime = 0;
    if (currentMillis - lastResponseTime < DELAY_AFTER_RESPONSE) return;
    lastResponseTime = currentMillis;

    if (!ctx->waitingForResponse) {
        rs422SendStatus();
        ctx->waitingForResponse = true;
    } else {
        uint8_t respBuffer[32] = {0};
        int respLength = rs422WaitForResponse(respBuffer, STATUS_RESPONSE_LENGTH, 'S');
        if (handleResponse(respBuffer, respLength, STATUS_RESPONSE_LENGTH, ctx)) {
            if (respBuffer[4] == '9' && respBuffer[5] == '0') {
                rs422SendNozzleOff();
                ctx->waitingForResponse = true;
                nozzleUpStartTime = 0;
            } else if (respBuffer[4] == '1' && respBuffer[5] == '0') {
                ctx->state = FSM_STATE_IDLE;
                ctx->stateEntryTime = currentMillis;
                ctx->nozzleUpWarning = false;
                if (ctx->modeSelected) {
                    displayFuelMode(ctx->fuelMode);
                } else {
                    displayMessage("Please select mode");
                }
                nozzleUpStartTime = 0;
            } else if (respBuffer[4] == '2' && respBuffer[5] == '1') {
                rs422SendNozzleOff();
                ctx->waitingForResponse = true;
                ctx->nozzleUpWarning = true;
                if (nozzleUpStartTime == 0) {
                    nozzleUpStartTime = currentMillis;
                }
                if (currentMillis - nozzleUpStartTime > 60000) {
                    ctx->state = FSM_STATE_ERROR;
                    ctx->stateEntryTime = currentMillis;
                    displayMessage("Nozzle up long! Check");
                } else {
                    displayMessage("Nozzle up! Hang up");
                }
            } else if (respBuffer[4] == '7' && respBuffer[5] == '1') {
                ctx->state = FSM_STATE_TRANSACTION_PAUSED;
                ctx->stateEntryTime = currentMillis;
                ctx->monitorActive = true;
                ctx->monitorState = 0;
                displayTransaction(ctx->currentLiters_dL, ctx->currentPriceTotal, "Paused", ctx->price > 9999);
                saveTransactionState(ctx->currentLiters_dL, ctx->currentPriceTotal, ctx->state, ctx->fuelMode, ctx->modeSelected);
            } else if (respBuffer[4] == '6' && respBuffer[5] == '1') {
                ctx->state = FSM_STATE_TRANSACTION;
                ctx->stateEntryTime = currentMillis;
                ctx->monitorActive = true;
                ctx->monitorState = 1;
                ctx->transactionStarted = true;
                rs422SendLitersMonitor();
                ctx->waitingForResponse = true;
                displayTransaction(ctx->currentLiters_dL, ctx->currentPriceTotal, "Restoring trans...", ctx->price > 9999);
            } else {
                ctx->errorCount++;
                if (ctx->errorCount >= MAX_ERROR_COUNT) {
                    ctx->state = FSM_STATE_ERROR;
                    ctx->stateEntryTime = currentMillis;
                    displayMessage("Pump Error");
                }
            }
        }
    }
}

static void updateError(FSMContext* ctx) {
    unsigned long currentMillis = getCurrentMillis();
    static unsigned long lastResponseTime = 0;
    if (currentMillis - lastResponseTime < DELAY_AFTER_RESPONSE) return;
    lastResponseTime = currentMillis;

    if (currentMillis - ctx->stateEntryTime >= RESPONSE_TIMEOUT) {
        rs422SendStatus();
        ctx->waitingForResponse = true;
        ctx->stateEntryTime = currentMillis;
        displayMessage("Pump offline! Check");
    }
    if (ctx->waitingForResponse) {
        uint8_t respBuffer[32] = {0};
        int respLength = rs422WaitForResponse(respBuffer, STATUS_RESPONSE_LENGTH, 'S');
        if (handleResponse(respBuffer, respLength, STATUS_RESPONSE_LENGTH, ctx)) {
            if (respBuffer[4] == '9' && respBuffer[5] == '0') {
                rs422SendNozzleOff();
                ctx->waitingForResponse = true;
            } else if (respBuffer[4] == '1' && respBuffer[5] == '0') {
                ctx->state = FSM_STATE_IDLE;
                ctx->stateEntryTime = currentMillis;
                ctx->nozzleUpWarning = false;
                ctx->transactionStarted = false;
                ctx->monitorActive = false;
                ctx->monitorState = 0;
                ctx->waitingForResponse = false;
                if (ctx->modeSelected) {
                    displayFuelMode(ctx->fuelMode);
                } else {
                    displayMessage("Please select mode");
                }
            } else if (respBuffer[4] == '2' && respBuffer[5] == '1') {
                rs422SendNozzleOff();
                ctx->waitingForResponse = true;
                ctx->nozzleUpWarning = true;
                displayMessage("Nozzle up! Hang up");
            } else if (respBuffer[4] == '7' && respBuffer[5] == '1') {
                ctx->state = FSM_STATE_TRANSACTION_PAUSED;
                ctx->stateEntryTime = currentMillis;
                ctx->monitorActive = true;
                ctx->monitorState = 0;
                displayTransaction(ctx->currentLiters_dL, ctx->currentPriceTotal, "Paused", ctx->price > 9999);
                saveTransactionState(ctx->currentLiters_dL, ctx->currentPriceTotal, ctx->state, ctx->fuelMode, ctx->modeSelected);
            } else {
                ctx->state = FSM_STATE_CHECK_STATUS;
                ctx->stateEntryTime = currentMillis;
                ctx->waitingForResponse = false;
            }
        }
    }
}

static void updateIdle(FSMContext* ctx) {
    unsigned long currentMillis = getCurrentMillis();
    static unsigned long lastResponseTime = 0;
    static unsigned long nozzleUpStartTime = 0;
    if (currentMillis - lastResponseTime < DELAY_AFTER_RESPONSE) return;
    lastResponseTime = currentMillis;

    // Принудительный сброс nozzleUpWarning через 3 секунды после входа в IDLE
    if (ctx->nozzleUpWarning && (currentMillis - ctx->stateEntryTime > 3000)) {
        ctx->nozzleUpWarning = false;
        ctx->errorCount = 0;
        logMessage(LOG_LEVEL_DEBUG, "Forced reset of nozzleUpWarning");
        if (ctx->modeSelected) {
            displayFuelMode(ctx->fuelMode);
        } else {
            displayMessage("Please select mode");
        }
    }

    if (ctx->skipFirstStatusCheck) {
        ctx->skipFirstStatusCheck = false;
        ctx->transactionStarted = false;
        ctx->monitorActive = false;
        ctx->monitorState = 0;
        ctx->waitingForResponse = false;
        if (ctx->modeSelected) {
            displayFuelMode(ctx->fuelMode);
        } else {
            displayMessage("Please select mode");
        }
        return;
    }
    if (ctx->statusPollingActive && !ctx->waitingForResponse) {
        rs422SendStatus();
        ctx->waitingForResponse = true;
    } else if (ctx->waitingForResponse) {
        uint8_t respBuffer[32] = {0};
        int respLength = rs422WaitForResponse(respBuffer, STATUS_RESPONSE_LENGTH, 'S');
        if (handleResponse(respBuffer, respLength, STATUS_RESPONSE_LENGTH, ctx)) {
            if (respBuffer[4] == '9' && respBuffer[5] == '0') {
                rs422SendNozzleOff();
                ctx->waitingForResponse = true;
                nozzleUpStartTime = 0;
                ctx->nozzleUpWarning = false;
            } else if (respBuffer[4] == '1' && respBuffer[5] == '0') {
                ctx->nozzleUpWarning = false;
                if (ctx->modeSelected) {
                    displayFuelMode(ctx->fuelMode);
                } else {
                    displayMessage("Please select mode");
                }
                nozzleUpStartTime = 0;
            } else if (respBuffer[4] == '2' && respBuffer[5] == '1') {
                rs422SendNozzleOff();
                ctx->waitingForResponse = true;
                ctx->nozzleUpWarning = true;
                if (nozzleUpStartTime == 0) {
                    nozzleUpStartTime = currentMillis;
                }
                displayMessage("Nozzle up! Hang up");
            } else {
                ctx->errorCount++;
                if (ctx->errorCount >= MAX_ERROR_COUNT) {
                    ctx->state = FSM_STATE_ERROR;
                    ctx->stateEntryTime = currentMillis;
                    displayMessage("Pump Error");
                }
            }
        }
    }
}

static void updateWaitForPriceInput(FSMContext* ctx) {
    // Пустая реализация, обработка в processKeyFSM
}

static void updateViewPrice(FSMContext* ctx) {
    unsigned long currentMillis = getCurrentMillis();
    if (currentMillis - ctx->stateEntryTime >= 10000) {
        ctx->state = FSM_STATE_IDLE;
        ctx->stateEntryTime = currentMillis;
        if (!ctx->nozzleUpWarning) {
            if (ctx->modeSelected) {
                displayFuelMode(ctx->fuelMode);
            } else {
                displayMessage("Please select mode");
            }
        }
    }
}

static void updateTransitionPriceSet(FSMContext* ctx) {
    unsigned long currentMillis = getCurrentMillis();
    if (currentMillis - ctx->stateEntryTime >= TRANSITION_TIMEOUT) {
        ctx->waitingForResponse = false;
        ctx->state = FSM_STATE_CHECK_STATUS;
        ctx->stateEntryTime = currentMillis;
    }
}

static void updateTransitionEditPrice(FSMContext* ctx) {
    unsigned long currentMillis = getCurrentMillis();
    if (currentMillis - ctx->stateEntryTime >= TRANSITION_TIMEOUT) {
        ctx->waitingForResponse = false;
        ctx->state = FSM_STATE_IDLE;
        ctx->stateEntryTime = currentMillis;
        if (!ctx->nozzleUpWarning) {
            if (ctx->modeSelected) {
                displayFuelMode(ctx->fuelMode);
            } else {
                displayMessage("Please select mode");
            }
        }
    }
}

static void updateEditPrice(FSMContext* ctx) {
    unsigned long currentMillis = getCurrentMillis();
    if (currentMillis - ctx->stateEntryTime >= EDIT_TIMEOUT) {
        ctx->state = FSM_STATE_IDLE;
        ctx->stateEntryTime = currentMillis;
        if (!ctx->nozzleUpWarning) {
            if (ctx->modeSelected) {
                displayFuelMode(ctx->fuelMode);
            } else {
                displayMessage("Please select mode");
            }
        }
    }
}

static void updateConfirmTransaction(FSMContext* ctx) {
    // Ждём действия пользователя, без таймаута
}

static void updateTransaction(FSMContext* ctx) {
    unsigned long currentMillis = getCurrentMillis();
    static unsigned long lastResponseTime = 0;
    if (currentMillis - lastResponseTime < DELAY_AFTER_RESPONSE) return;
    lastResponseTime = currentMillis;

    if (!ctx->waitingForResponse) {
        if (!ctx->transactionStarted) {
            rs422SendStatus();
            ctx->waitingForResponse = true;
        } else {
            if (!ctx->monitorActive) {
                rs422SendStatus();
                ctx->waitingForResponse = true;
            } else {
                switch (ctx->monitorState) {
                    case 0: rs422SendStatus(); break;
                    case 1: rs422SendLitersMonitor(); break;
                    case 2: rs422SendRevenueStatus(); break;
                }
                ctx->waitingForResponse = true;
            }
        }
    } else {
        uint8_t respBuffer[32] = {0};
        int expectedLength = ctx->monitorActive ? (ctx->monitorState == 0 ? STATUS_RESPONSE_LENGTH : MONITOR_RESPONSE_LENGTH) : STATUS_RESPONSE_LENGTH;
        char expectedCommand = ctx->monitorState == 0 ? 'S' : (ctx->monitorState == 1 ? 'L' : 'R');
        int respLength = rs422WaitForResponse(respBuffer, expectedLength, expectedCommand);
        if (handleResponse(respBuffer, respLength, STATUS_RESPONSE_LENGTH, ctx)) {
            if (!ctx->transactionStarted && respBuffer[4] == '2' && respBuffer[5] == '1') { // Только S21
                uint16_t protocolPrice = ctx->price > 9999 ? ctx->price / 10 : ctx->price;
                rs422SendTransaction(ctx->fuelMode, ctx->transactionVolume, ctx->transactionAmount, protocolPrice);
                ctx->waitingForResponse = true;
                ctx->transactionStarted = true;
                ctx->currentLiters_dL = 0;
                ctx->currentPriceTotal = 0;
                ctx->errorCount = 0;
                displayTransaction(ctx->currentLiters_dL, ctx->currentPriceTotal, "Dispensing...", ctx->price > 9999);
                logMessage(LOG_LEVEL_DEBUG, "Transaction started");
            } else if (ctx->monitorState == 0) {
                if (isValidStatus(respBuffer)) {
                    for (size_t i = 0; i < sizeof(statusActions) / sizeof(statusActions[0]); i++) {
                        if (respBuffer[4] == statusActions[i].code[0] && respBuffer[5] == statusActions[i].code[1]) {
                            ctx->state = statusActions[i].nextState;
                            ctx->stateEntryTime = currentMillis;
                            if (statusActions[i].resetErrorCount) ctx->errorCount = 0;
                            if (statusActions[i].nextState == FSM_STATE_TRANSACTION_END) {
                                rs422SendTransactionUpdate();
                                ctx->waitingForResponse = true;
                                if (respBuffer[4] == '9' && respBuffer[5] == '0') {
                                    rs422SendNozzleOff();
                                }
                                displayTransaction(ctx->currentLiters_dL, ctx->currentPriceTotal, "Trans stopped", ctx->price > 9999);
                                saveTransactionState(ctx->currentLiters_dL, ctx->currentPriceTotal, ctx->state, ctx->fuelMode, ctx->modeSelected);
                            } else if (statusActions[i].nextState == FSM_STATE_TRANSACTION_PAUSED) {
                                displayTransaction(ctx->currentLiters_dL, ctx->currentPriceTotal, "Paused", ctx->price > 9999);
                                saveTransactionState(ctx->currentLiters_dL, ctx->currentPriceTotal, ctx->state, ctx->fuelMode, ctx->modeSelected);
                            } else if (statusActions[i].nextState == FSM_STATE_TRANSACTION && respBuffer[4] == '6' && respBuffer[5] == '1') {
                                ctx->monitorActive = true;
                                ctx->monitorState = 1;
                                rs422SendLitersMonitor();
                                ctx->waitingForResponse = true;
                            }
                            break;
                        }
                    }
                }
            } else if (ctx->monitorActive) {
                if (ctx->monitorState == 1 && respBuffer[3] == 'L' && respBuffer[4] == '1') {
                    char litersStr[7] = {0};
                    if (respLength >= 14) {
                        memcpy(litersStr, respBuffer + 8, 6);
                        bool valid = true;
                        for (int i = 0; i < 6; i++) {
                            if (litersStr[i] < '0' || litersStr[i] > '9') {
                                valid = false;
                                break;
                            }
                        }
                        ctx->currentLiters_dL = valid ? atol(litersStr) : ctx->currentLiters_dL;
                        displayTransaction(ctx->currentLiters_dL, ctx->currentPriceTotal, "Dispensing...", ctx->price > 9999);
                    }
                    ctx->monitorState = 2;
                } else if (ctx->monitorState == 2 && respBuffer[3] == 'R' && respBuffer[4] == '1') {
                    char priceStr[7] = {0};
                    if (respLength >= 14) {
                        memcpy(priceStr, respBuffer + 8, 6);
                        bool valid = true;
                        for (int i = 0; i < 6; i++) {
                            if (priceStr[i] < '0' || priceStr[i] > '9') {
                                valid = false;
                                break;
                            }
                        }
                        ctx->currentPriceTotal = valid ? atol(priceStr) : ctx->currentPriceTotal;
                        displayTransaction(ctx->currentLiters_dL, ctx->currentPriceTotal, "Dispensing...", ctx->price > 9999);
                    }
                    ctx->monitorState = 0;
                }
            }
        }
    }
}

static void updateTransactionPaused(FSMContext* ctx) {
    unsigned long currentMillis = getCurrentMillis();
    static unsigned long lastResponseTime = 0;
    if (currentMillis - lastResponseTime < DELAY_AFTER_RESPONSE) return;
    lastResponseTime = currentMillis;

    if (currentMillis - ctx->stateEntryTime > 30000) {
        ctx->finalLiters_dL = ctx->currentLiters_dL;
        ctx->finalPriceTotal = ctx->currentPriceTotal;
        rs422SendTransactionUpdate();
        ctx->waitingForResponse = true;
        ctx->state = FSM_STATE_TRANSACTION_END;
        ctx->stateEntryTime = currentMillis;
        displayMessage("Nozzle back! Trans end");
        saveTransactionState(ctx->finalLiters_dL, ctx->finalPriceTotal, ctx->state, ctx->fuelMode, ctx->modeSelected);
        return;
    }

    if (!ctx->waitingForResponse) {
        rs422SendStatus();
        ctx->waitingForResponse = true;
    } else {
        uint8_t respBuffer[32] = {0};
        int respLength = rs422WaitForResponse(respBuffer, STATUS_RESPONSE_LENGTH, 'S');
        if (handleResponse(respBuffer, respLength, STATUS_RESPONSE_LENGTH, ctx)) {
            if (respBuffer[4] == '9' && respBuffer[5] == '0') {
                ctx->finalLiters_dL = ctx->currentLiters_dL;
                ctx->finalPriceTotal = ctx->currentPriceTotal;
                rs422SendTransactionUpdate();
                ctx->waitingForResponse = true;
                ctx->state = FSM_STATE_TRANSACTION_END;
                ctx->stateEntryTime = currentMillis;
                saveTransactionState(ctx->finalLiters_dL, ctx->finalPriceTotal, ctx->state, ctx->fuelMode, ctx->modeSelected);
            } else if (respBuffer[4] != '7' || respBuffer[5] != '1') {
                ctx->monitorActive = true;
                ctx->monitorState = 0;
                ctx->state = FSM_STATE_TRANSACTION;
                ctx->stateEntryTime = currentMillis;
                displayTransaction(ctx->currentLiters_dL, ctx->currentPriceTotal, "Dispensing...", ctx->price > 9999);
            }
        }
    }
}

static void updateTransactionEnd(FSMContext* ctx) {
    unsigned long currentMillis = getCurrentMillis();
    static unsigned long lastResponseTime = 0;
    static bool dataReceived = false;
    static uint8_t retryCount = 0;

    if (ctx->stateEntryTime == currentMillis) {
        dataReceived = false;
        retryCount = 0;
    }

    if (currentMillis - lastResponseTime < DELAY_AFTER_RESPONSE) return;
    lastResponseTime = currentMillis;

    if (!ctx->waitingForResponse && !dataReceived && retryCount < 5) {
        rs422SendTransactionUpdate();
        ctx->waitingForResponse = true;
        retryCount++;
        char logMsg[64];
        snprintf(logMsg, sizeof(logMsg), "Requesting transaction update, attempt: %d", retryCount);
        logMessage(LOG_LEVEL_DEBUG, logMsg);
    } else if (ctx->waitingForResponse) {
        uint8_t respBuffer[32] = {0};
        int respLength = rs422WaitForResponse(respBuffer, TRANSACTION_END_RESPONSE_LENGTH, 'T');
        if (respLength >= 18) {
            ctx->waitingForResponse = false;
            ctx->errorCount = 0;
            if (respBuffer[3] == 'T' && respBuffer[4] == '1') {
                char priceStr[7] = {0};
                char litersStr[7] = {0};
                int offset = (respBuffer[5] == 'u') ? 10 : 8;
                memcpy(priceStr, respBuffer + offset, 6);
                memcpy(litersStr, respBuffer + offset + 7, 6);
                bool valid = true;
                for (int i = 0; i < 6; i++) {
                    if (litersStr[i] < '0' || litersStr[i] > '9' || priceStr[i] < '0' || priceStr[i] > '9') {
                        valid = false;
                        break;
                    }
                }
                if (valid) {
                    ctx->finalLiters_dL = atol(litersStr);
                    ctx->finalPriceTotal = atol(priceStr);
                } else {
                    logMessage(LOG_LEVEL_ERROR, "Invalid transaction data, using last valid values");
                }
                displayTransaction(ctx->finalLiters_dL, ctx->finalPriceTotal, "Filling end", ctx->price > 9999);
                rs422SendNozzleOff();
                ctx->waitingForResponse = false;
                dataReceived = true;
                retryCount = 0;
                saveTransactionState(ctx->finalLiters_dL, ctx->finalPriceTotal, ctx->state, ctx->fuelMode, ctx->modeSelected);
                char logMsg[64];
                snprintf(logMsg, sizeof(logMsg), "Transaction end: Liters=%lu, Price=%lu",
                         (unsigned long)ctx->finalLiters_dL, (unsigned long)ctx->finalPriceTotal);
                logMessage(LOG_LEVEL_DEBUG, logMsg);
            }
        } else {
            ctx->waitingForResponse = false;
            ctx->errorCount++;
            retryCount++;
            if (retryCount >= 5) {
                ctx->state = FSM_STATE_ERROR;
                ctx->stateEntryTime = currentMillis;
                displayMessage("Trans error! Check pump");
                logMessage(LOG_LEVEL_ERROR, "Transaction data error after retries");
            }
        }
    }
}

static void updateTotalCounter(FSMContext* ctx) {
    unsigned long currentMillis = getCurrentMillis();
    static unsigned long lastResponseTime = 0;
    if (currentMillis - lastResponseTime < DELAY_AFTER_RESPONSE) return;
    lastResponseTime = currentMillis;

    if (!ctx->waitingForResponse && ctx->c0RetryCount < MAX_ERROR_COUNT && (currentMillis - ctx->lastC0SendTime) >= RESPONSE_TIMEOUT) {
        rs422SendTotalCounter();
        ctx->waitingForResponse = true;
        ctx->lastC0SendTime = currentMillis;
        ctx->c0RetryCount++;
    } else if (ctx->waitingForResponse) {
        uint8_t respBuffer[32] = {0};
        int respLength = rs422WaitForResponse(respBuffer, TOTAL_COUNTER_RESPONSE_LENGTH, 'C');
        if (handleResponse(respBuffer, respLength, TOTAL_COUNTER_RESPONSE_LENGTH, ctx)) {
            if (respBuffer[3] == 'C' && respBuffer[4] == '1') {
                char totalStr[10] = {0};
                memcpy(totalStr, respBuffer + 6, 9);
                bool valid = true;
                for (int i = 0; i < 9; i++) {
                    if (totalStr[i] < '0' || totalStr[i] > '9') {
                        valid = false;
                        break;
                    }
                }
                if (valid) {
                    uint32_t totalLiters_mL = atol(totalStr);
                    char litersBuf[12];
                    formatLiters(totalLiters_mL / 10, litersBuf, sizeof(litersBuf));
                    char displayStr[32];
                    snprintf(displayStr, sizeof(displayStr), "TOTAL:\n%s", litersBuf);
                    displayMessage(displayStr);
                } else {
                    displayMessage("TOTAL:\nError");
                }
                ctx->waitingForResponse = false;
                ctx->c0RetryCount = MAX_ERROR_COUNT;
            } else {
                if (ctx->c0RetryCount >= MAX_ERROR_COUNT) {
                    displayMessage("TOTAL:\nError");
                }
            }
        }
    }
}

// Инициализация FSM
void initFSM(FSMContext* ctx)
{
    // Инициализация мьютекса для логов
    logMutex = xSemaphoreCreateMutex();
    if (logMutex == NULL) {
        Error_Handler();
    }

    // Инициализация логов через UART3
    char logMsg[32];
    snprintf(logMsg, sizeof(logMsg), "FSM Initialized\r\n");
    logMessage(LOG_LEVEL_DEBUG, logMsg);

    // Отправка команды Nozzle Off
    rs422SendNozzleOff();

    // Чтение цены из EEPROM
    ctx->price = readPriceFromEEPROM();
    ctx->priceValid = ctx->price > 0;
    ctx->fuelMode = FUEL_BY_VOLUME;
    ctx->stateEntryTime = getCurrentMillis();
    ctx->waitingForResponse = false;
    ctx->errorCount = 0;
    ctx->c0RetryCount = 0;
    ctx->lastC0SendTime = 0;
    ctx->statusPollingActive = true;
    ctx->transactionVolume = 0;
    ctx->transactionAmount = 0;
    ctx->transactionStarted = false;
    ctx->monitorState = 0;
    ctx->monitorActive = false;
    ctx->currentLiters_dL = 0;
    ctx->finalLiters_dL = 0;
    ctx->currentPriceTotal = 0;
    ctx->finalPriceTotal = 0;
    ctx->nozzleUpWarning = false;
    ctx->skipFirstStatusCheck = false;
    ctx->lastKeyTime = 0;
    ctx->priceInput[0] = '\0';
    ctx->modeSelected = false;

    // Проверка сохранённой транзакции
    uint32_t savedLiters, savedPrice;
    FSMState savedState;
    FuelMode savedMode;
    bool savedModeSelected;
    if (restoreTransactionState(&savedLiters, &savedPrice, &savedState, &savedMode, &savedModeSelected)) {
        ctx->currentLiters_dL = savedLiters;
        ctx->currentPriceTotal = savedPrice;
        ctx->state = savedState;
        if (savedState == FSM_STATE_TRANSACTION || savedState == FSM_STATE_TRANSACTION_PAUSED) {
            ctx->fuelMode = savedMode;
            ctx->modeSelected = savedModeSelected;
            ctx->transactionStarted = true;
            ctx->monitorActive = true;
            ctx->monitorState = 1;
            displayTransaction(ctx->currentLiters_dL, ctx->currentPriceTotal, "Restoring trans...", ctx->price > 9999);
        } else {
            // Игнорируем сохранённый режим для неактивных транзакций
            ctx->state = ctx->priceValid ? FSM_STATE_CHECK_STATUS : FSM_STATE_WAIT_FOR_PRICE_INPUT;
            if (!ctx->priceValid) {
                displayMessage("Set price (0-99999)");
            } else {
                displayMessage("Please select mode");
            }
        }
    } else {
        ctx->state = ctx->priceValid ? FSM_STATE_CHECK_STATUS : FSM_STATE_WAIT_FOR_PRICE_INPUT;
        if (!ctx->priceValid) {
            displayMessage("Set price (0-99999)");
        } else {
            displayMessage("Please select mode");
        }
    }

    if (ctx->state == FSM_STATE_CHECK_STATUS) {
        rs422SendStatus();
        ctx->waitingForResponse = true;
    }

    // Отображение приветственного сообщения
    displayMessage("CENSTAR");
    osDelay(DISPLAY_WELCOME_DURATION);
}

// Основной цикл FSM
void updateFSM(FSMContext* ctx)
{
    switch (ctx->state) {
        case FSM_STATE_CHECK_STATUS:        updateCheckStatus(ctx); break;
        case FSM_STATE_ERROR:               updateError(ctx); break;
        case FSM_STATE_IDLE:                updateIdle(ctx); break;
        case FSM_STATE_WAIT_FOR_PRICE_INPUT: updateWaitForPriceInput(ctx); break;
        case FSM_STATE_VIEW_PRICE:          updateViewPrice(ctx); break;
        case FSM_STATE_TRANSITION_PRICE_SET: updateTransitionPriceSet(ctx); break;
        case FSM_STATE_TRANSITION_EDIT_PRICE: updateTransitionEditPrice(ctx); break;
        case FSM_STATE_EDIT_PRICE:          updateEditPrice(ctx); break;
        case FSM_STATE_CONFIRM_TRANSACTION: updateConfirmTransaction(ctx); break;
        case FSM_STATE_TRANSACTION:         updateTransaction(ctx); break;
        case FSM_STATE_TRANSACTION_PAUSED:  updateTransactionPaused(ctx); break;
        case FSM_STATE_TRANSACTION_END:     updateTransactionEnd(ctx); break;
        case FSM_STATE_TOTAL_COUNTER:       updateTotalCounter(ctx); break;
        default: break;
    }
}

// Управление вводом клавиш
void processKeyFSM(FSMContext* ctx, char key)
{
    unsigned long currentMillis = getCurrentMillis();
    if (currentMillis - ctx->lastKeyTime < KEY_DEBOUNCE_MS) {
        displayMessage("Slow down! Wait");
        return;
    }
    ctx->lastKeyTime = currentMillis;

    char logMsg[32];
    snprintf(logMsg, sizeof(logMsg), "Key pressed: %c", key);
    logMessage(LOG_LEVEL_DEBUG, logMsg);
    snprintf(logMsg, sizeof(logMsg), "Mode selected: %d", ctx->modeSelected);
    logMessage(LOG_LEVEL_DEBUG, logMsg);
    snprintf(logMsg, sizeof(logMsg), "Current mode: %d", ctx->fuelMode);
    logMessage(LOG_LEVEL_DEBUG, logMsg);

    switch (ctx->state) {
        case FSM_STATE_WAIT_FOR_PRICE_INPUT: {
            if (key >= '0' && key <= '9') {
                size_t len = strlen(ctx->priceInput);
                if (len < PRICE_FORMAT_LENGTH) {
                    ctx->priceInput[len] = key;
                    ctx->priceInput[len + 1] = '\0';
                    char displayStr[32];
                    snprintf(displayStr, sizeof(displayStr), "%s: %s",
                             ctx->fuelMode == FUEL_BY_VOLUME ? "Volume" : "Amount", ctx->priceInput);
                    displayMessage(displayStr);
                    snprintf(logMsg, sizeof(logMsg), "Input so far: %s", ctx->priceInput);
                    logMessage(LOG_LEVEL_DEBUG, logMsg);
                }
                ctx->stateEntryTime = currentMillis;
            }
            else if (key == '*') {
                size_t len = strlen(ctx->priceInput);
                if (len < PRICE_FORMAT_LENGTH - 1 && strchr(ctx->priceInput, '.') == NULL) {
                    ctx->priceInput[len] = '.';
                    ctx->priceInput[len + 1] = '\0';
                    char displayStr[32];
                    snprintf(displayStr, sizeof(displayStr), "%s: %s",
                             ctx->fuelMode == FUEL_BY_VOLUME ? "Volume" : "Amount", ctx->priceInput);
                    displayMessage(displayStr);
                    snprintf(logMsg, sizeof(logMsg), "Input so far: %s", ctx->priceInput);
                    logMessage(LOG_LEVEL_DEBUG, logMsg);
                }
                ctx->stateEntryTime = currentMillis;
            }
            else if (key == 'E') {
                if (strlen(ctx->priceInput) == 0) {
                    ctx->state = FSM_STATE_IDLE;
                    ctx->stateEntryTime = currentMillis;
                    if (!ctx->nozzleUpWarning) {
                        if (ctx->modeSelected) {
                            displayFuelMode(ctx->fuelMode);
                        } else {
                            displayMessage("Please select mode");
                        }
                    }
                } else {
                    ctx->priceInput[0] = '\0';
                    displayMessage("Cleared");
                    ctx->stateEntryTime = currentMillis;
                }
            }
            else if (key == 'K') {
                if (strlen(ctx->priceInput) > 0) {
                    uint32_t value;
                    if (ctx->fuelMode == FUEL_BY_VOLUME) {
                        float floatValue = atof(ctx->priceInput);
                        if (floatValue > 0 && floatValue <= 9999.99) {
                            value = (uint32_t)(floatValue * 100);
                        } else {
                            displayMessage("Invalid volume!");
                            ctx->priceInput[0] = '\0';
                            ctx->stateEntryTime = currentMillis;
                            logMessage(LOG_LEVEL_ERROR, "Invalid volume: Out of range");
                            break;
                        }
                    } else {
                        value = atol(ctx->priceInput);
                        if (value == 0) {
                            displayMessage("Invalid amount!");
                            ctx->priceInput[0] = '\0';
                            ctx->stateEntryTime = currentMillis;
                            logMessage(LOG_LEVEL_ERROR, "Invalid amount: Zero");
                            break;
                        }
                        snprintf(logMsg, sizeof(logMsg), "Parsed amount: %lu", (unsigned long)value);
                        logMessage(LOG_LEVEL_DEBUG, logMsg);
                    }
                    if (ctx->fuelMode == FUEL_BY_VOLUME) {
                        ctx->transactionVolume = value;
                        ctx->transactionAmount = 0;
                    } else {
                        ctx->transactionVolume = 0;
                        ctx->transactionAmount = value;
                    }
                    ctx->state = FSM_STATE_CONFIRM_TRANSACTION;
                    ctx->stateEntryTime = currentMillis;
                    ctx->priceInput[0] = '\0';
                    displayMessage("Confirm? Press K");
                    snprintf(logMsg, sizeof(logMsg), "Confirmed value: %lu", (unsigned long)value);
                    logMessage(LOG_LEVEL_DEBUG, logMsg);
                }
            }
            break;
        }
        case FSM_STATE_IDLE: {
            if (ctx->nozzleUpWarning && key == 'K') {
                displayMessage("Nozzle up! Hang up");
                ctx->stateEntryTime = currentMillis;
            } else if (key == 'G') {
                ctx->state = FSM_STATE_VIEW_PRICE;
                ctx->stateEntryTime = currentMillis;
                char priceStr[16];
                snprintf(priceStr, sizeof(priceStr), "Price: %u", ctx->price);
                displayMessage(priceStr);
            } else if (key == 'E') {
                ctx->statusPollingActive = true;
                ctx->modeSelected = false;
                if (!ctx->nozzleUpWarning) {
                    displayMessage("Please select mode");
                }
                ctx->stateEntryTime = currentMillis;
            } else if (key == 'C') {
                ctx->fuelMode = (FuelMode)((ctx->fuelMode + 1) % 3);
                ctx->modeSelected = true;
                displayFuelMode(ctx->fuelMode);
                ctx->stateEntryTime = currentMillis;
            } else if (key == 'K' && !ctx->nozzleUpWarning) {
                if (ctx->fuelMode == FUEL_BY_VOLUME || ctx->fuelMode == FUEL_BY_PRICE) {
                    ctx->priceInput[0] = '\0';
                    ctx->state = FSM_STATE_WAIT_FOR_PRICE_INPUT;
                    displayMessage(ctx->fuelMode == FUEL_BY_VOLUME ? "Enter Volume" : "Enter Amount");
                } else {
                    ctx->transactionVolume = 0;
                    ctx->transactionAmount = 999999;
                    ctx->state = FSM_STATE_CONFIRM_TRANSACTION;
                    ctx->stateEntryTime = currentMillis;
                    displayMessage("Confirm? Press K");
                }
            } else if (key == 'A') {
                ctx->statusPollingActive = false;
                ctx->state = FSM_STATE_TOTAL_COUNTER;
                ctx->stateEntryTime = currentMillis;
                ctx->errorCount = 0;
                ctx->c0RetryCount = 0;
                ctx->waitingForResponse = true;
                ctx->lastC0SendTime = currentMillis;
                rs422SendTotalCounter();
                displayMessage("TOTAL:\nWaiting...");
            }
            break;
        }
        case FSM_STATE_VIEW_PRICE: {
            if (key == 'G') {
                ctx->state = FSM_STATE_EDIT_PRICE;
                ctx->stateEntryTime = currentMillis;
                ctx->priceInput[0] = '\0';
                displayMessage("Editing Price");
            } else if (key == 'E') {
                ctx->state = FSM_STATE_IDLE;
                ctx->stateEntryTime = currentMillis;
                if (!ctx->nozzleUpWarning) {
                    if (ctx->modeSelected) {
                        displayFuelMode(ctx->fuelMode);
                    } else {
                        displayMessage("Please select mode");
                    }
                }
            }
            break;
        }
        case FSM_STATE_EDIT_PRICE: {
            ctx->stateEntryTime = currentMillis;
            if (key >= '0' && key <= '9') {
                size_t len = strlen(ctx->priceInput);
                if (len < PRICE_FORMAT_LENGTH) {
                    ctx->priceInput[len] = key;
                    ctx->priceInput[len + 1] = '\0';
                    char displayStr[32];
                    snprintf(displayStr, sizeof(displayStr), "New Price: %s", ctx->priceInput);
                    displayMessage(displayStr);
                }
            } else if (key == 'E') {
                ctx->priceInput[0] = '\0';
                displayMessage("Price cleared");
            } else if (key == 'K') {
                if (strlen(ctx->priceInput) > 0) {
                    uint16_t newPrice = atol(ctx->priceInput);
                    if (newPrice >= PRICE_MIN && newPrice <= 99999) {
                        ctx->price = newPrice;
                        writePriceToEEPROM(ctx->price);
                        displayMessage("Price updated!");
                        ctx->state = FSM_STATE_TRANSITION_EDIT_PRICE;
                        ctx->stateEntryTime = currentMillis;
                        ctx->priceInput[0] = '\0';
                    } else {
                        displayMessage("Price too high! Max");
                        ctx->priceInput[0] = '\0';
                    }
                } else {
                    ctx->state = FSM_STATE_IDLE;
                    if (!ctx->nozzleUpWarning) {
                        if (ctx->modeSelected) {
                            displayFuelMode(ctx->fuelMode);
                        } else {
                            displayMessage("Please select mode");
                        }
                    }
                    ctx->stateEntryTime = currentMillis;
                }
            }
            break;
        }
        case FSM_STATE_TRANSITION_PRICE_SET:
        case FSM_STATE_TRANSITION_EDIT_PRICE: {
            if (currentMillis - ctx->stateEntryTime >= TRANSITION_TIMEOUT) {
                ctx->waitingForResponse = false;
                ctx->state = (ctx->state == FSM_STATE_TRANSITION_PRICE_SET) ? FSM_STATE_CHECK_STATUS : FSM_STATE_IDLE;
                ctx->stateEntryTime = currentMillis;
                if (ctx->state == FSM_STATE_IDLE && !ctx->nozzleUpWarning) {
                    if (ctx->modeSelected) {
                        displayFuelMode(ctx->fuelMode);
                    } else {
                        displayMessage("Please select mode");
                    }
                }
            }
            break;
        }
        case FSM_STATE_CONFIRM_TRANSACTION: {
            if (key == 'K') {
                ctx->state = FSM_STATE_TRANSACTION;
                ctx->stateEntryTime = currentMillis;
                displayMessage("Confirm! UP Nozzle");
                logMessage(LOG_LEVEL_DEBUG, "Transaction confirmed");
            } else if (key == 'E') {
                ctx->state = FSM_STATE_IDLE;
                ctx->stateEntryTime = currentMillis;
                ctx->transactionVolume = 0;
                ctx->transactionAmount = 0;
                ctx->nozzleUpWarning = false;
                ctx->waitingForResponse = false;
                ctx->errorCount = 0;
                if (ctx->modeSelected) {
                    displayFuelMode(ctx->fuelMode);
                } else {
                    displayMessage("Please select mode");
                }
                logMessage(LOG_LEVEL_DEBUG, "Confirm cancelled, returning to idle");
            }
            break;
        }
        case FSM_STATE_TRANSACTION: {
            if (key == 'E' && !ctx->transactionStarted) {
                rs422SendNozzleOff();
                ctx->waitingForResponse = false;
                ctx->statusPollingActive = false;
                ctx->state = FSM_STATE_IDLE;
                ctx->stateEntryTime = currentMillis;
                ctx->transactionStarted = false;
                ctx->monitorState = 0;
                ctx->monitorActive = false;
                ctx->currentLiters_dL = 0;
                ctx->currentPriceTotal = 0;
                ctx->finalLiters_dL = 0;
                ctx->finalPriceTotal = 0;
                ctx->errorCount = 0;
                ctx->nozzleUpWarning = false;
                ctx->skipFirstStatusCheck = true;
                ctx->transactionVolume = 0;
                ctx->transactionAmount = 0;
                rs422SendNozzleOff();
                osDelay(100);
                ctx->statusPollingActive = true;
                rs422SendStatus();
                if (!ctx->nozzleUpWarning) {
                    if (ctx->modeSelected) {
                        displayFuelMode(ctx->fuelMode);
                    } else {
                        displayMessage("Please select mode");
                    }
                }
                logMessage(LOG_LEVEL_DEBUG, "Transaction cancelled, returning to idle");
            } else if (key == 'E') {
                rs422SendPause();
                ctx->waitingForResponse = true;
                ctx->state = FSM_STATE_TRANSACTION_PAUSED;
                ctx->stateEntryTime = currentMillis;
                displayTransaction(ctx->currentLiters_dL, ctx->currentPriceTotal, "Paused", ctx->price > 9999);
                saveTransactionState(ctx->currentLiters_dL, ctx->currentPriceTotal, ctx->state, ctx->fuelMode, ctx->modeSelected);
                logMessage(LOG_LEVEL_DEBUG, "Transaction paused");
            }
            break;
        }
        case FSM_STATE_TRANSACTION_PAUSED: {
            if (key == 'K') {
                rs422SendResume();
                ctx->waitingForResponse = true;
                ctx->state = FSM_STATE_TRANSACTION;
                ctx->stateEntryTime = currentMillis;
                ctx->monitorActive = true;
                ctx->monitorState = 0;
                displayTransaction(ctx->currentLiters_dL, ctx->currentPriceTotal, "Dispensing...", ctx->price > 9999);
                logMessage(LOG_LEVEL_DEBUG, "Transaction resumed");
            } else if (key == 'E') {
                ctx->finalLiters_dL = ctx->currentLiters_dL;
                ctx->finalPriceTotal = ctx->currentPriceTotal;
                rs422SendTransactionUpdate();
                ctx->waitingForResponse = true;
                ctx->state = FSM_STATE_TRANSACTION_END;
                ctx->stateEntryTime = currentMillis;
                saveTransactionState(ctx->finalLiters_dL, ctx->finalPriceTotal, ctx->state, ctx->fuelMode, ctx->modeSelected);
                logMessage(LOG_LEVEL_DEBUG, "Transaction ended from paused");
            }
            break;
        }
        case FSM_STATE_TRANSACTION_END: {
            if (key == 'E') {
                ctx->state = FSM_STATE_IDLE;
                ctx->stateEntryTime = currentMillis;
                ctx->transactionStarted = false;
                ctx->monitorState = 0;
                ctx->monitorActive = false;
                ctx->waitingForResponse = false;
                ctx->currentLiters_dL = 0;
                ctx->currentPriceTotal = 0;
                ctx->finalLiters_dL = 0;
                ctx->finalPriceTotal = 0;
                ctx->errorCount = 0;
                ctx->skipFirstStatusCheck = true;
                ctx->nozzleUpWarning = false;
                ctx->transactionVolume = 0;
                ctx->transactionAmount = 0;
                if (ctx->modeSelected) {
                    displayFuelMode(ctx->fuelMode);
                } else {
                    displayMessage("Please select mode");
                }
                logMessage(LOG_LEVEL_DEBUG, "Transaction end, returning to idle");
            }
            break;
        }
        case FSM_STATE_TOTAL_COUNTER: {
            if (key == 'E') {
                ctx->state = FSM_STATE_IDLE;
                ctx->stateEntryTime = currentMillis;
                ctx->transactionStarted = false;
                ctx->monitorState = 0;
                ctx->monitorActive = false;
                ctx->waitingForResponse = false;
                ctx->errorCount = 0;
                ctx->c0RetryCount = 0;
                ctx->nozzleUpWarning = false;
                if (ctx->modeSelected) {
                    displayFuelMode(ctx->fuelMode);
                } else {
                    displayMessage("Please select mode");
                }
                logMessage(LOG_LEVEL_DEBUG, "Total counter cancelled, returning to idle");
            }
            break;
        }
        default: break;
    }
}

// Получение состояния FSM
FSMState getCurrentState(const FSMContext* ctx) {
    return ctx->state;
}

FuelMode getCurrentFuelMode(const FSMContext* ctx) {
    return ctx->fuelMode;
}

// Функция логирования через UART3
void logMessage(int level, const char* msg) {
    if (level >= LOG_LEVEL) {
        extern UART_HandleTypeDef huart3;
        char logMsg[128];
        snprintf(logMsg, sizeof(logMsg), "[%lu] %s\r\n", getCurrentMillis(), msg);
        if (xSemaphoreTake(logMutex, portMAX_DELAY) == pdTRUE) {
            HAL_UART_Transmit(&huart3, (uint8_t*)logMsg, strlen(logMsg), HAL_MAX_DELAY);
            xSemaphoreGive(logMutex);
        }
    }
}
