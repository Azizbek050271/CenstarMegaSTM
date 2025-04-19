/* fsm.h - Заголовочный файл для конечного автомата (FSM) */

#ifndef FSM_H
#define FSM_H

#include "stm32f4xx_hal.h"
#include "config.h"
#include <stdbool.h>
#include "FreeRTOS.h"
#include "semphr.h"

#define portMAX_DELAY ( TickType_t ) 0xffffffffUL

// Типы данных
typedef enum {
    FUEL_BY_VOLUME,
    FUEL_BY_PRICE,
    FUEL_BY_FULL_TANK
} FuelMode;

typedef enum {
    FSM_STATE_CHECK_STATUS,
    FSM_STATE_IDLE,
    FSM_STATE_WAIT_FOR_PRICE_INPUT,
    FSM_STATE_VIEW_PRICE,
    FSM_STATE_TRANSITION_PRICE_SET,
    FSM_STATE_EDIT_PRICE,
    FSM_STATE_TRANSITION_EDIT_PRICE,
    FSM_STATE_ERROR,
    FSM_STATE_TRANSACTION,
    FSM_STATE_TRANSACTION_END,
    FSM_STATE_TOTAL_COUNTER,
    FSM_STATE_TRANSACTION_PAUSED,
    FSM_STATE_CONFIRM_TRANSACTION
} FSMState;

typedef struct {
    FSMState state;
    FuelMode fuelMode;
    uint16_t price;
    bool priceValid;
    uint32_t transactionVolume;
    uint32_t transactionAmount;
    bool transactionStarted;
    bool waitingForResponse;
    int errorCount;
    int c0RetryCount;
    bool statusPollingActive;
    int monitorState;
    bool monitorActive;
    uint32_t currentLiters_dL;
    uint32_t finalLiters_dL;
    uint32_t currentPriceTotal;
    uint32_t finalPriceTotal;
    bool nozzleUpWarning;
    unsigned long stateEntryTime;
    unsigned long lastKeyTime;
    unsigned long lastC0SendTime;
    bool skipFirstStatusCheck;
    char priceInput[PRICE_FORMAT_LENGTH + 1];
    bool modeSelected;
} FSMContext;

// Прототипы функций
void initFSM(FSMContext* ctx);
void updateFSM(FSMContext* ctx);
void processKeyFSM(FSMContext* ctx, char key);
FSMState getCurrentState(const FSMContext* ctx);
FuelMode getCurrentFuelMode(const FSMContext* ctx);

// Функция для получения текущего времени (замена millis())
uint32_t getCurrentMillis(void);

// Функция логирования через UART3
void logMessage(int level, const char* msg);

#endif /* FSM_H */
