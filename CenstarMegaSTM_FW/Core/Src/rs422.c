/* rs422.c - Реализация коммуникации по RS-422 через UART2 с DMA */

#include "rs422.h"
#include "frame.h"
#include "config.h"
#include "crc.h"
#include "oled.h"
#include <stdio.h>
#include <string.h>
#include <cmsis_os.h>
#include <stdbool.h>

extern UART_HandleTypeDef huart2;
extern QueueHandle_t rs422TxQueue;
extern QueueHandle_t rs422RxQueue;

static const uint8_t slaveAddress[2] = {0x00, POST_ADDRESS};
static bool isSending = false;
static bool isReceiving = false;

static uint8_t rxBuffer[32]; // Буфер для приёма данных

// Инициализация RS-422
void initRS422(void) {
    // UART2 уже инициализирован в main.c
    // Запускаем приём через DMA
    HAL_UART_Receive_DMA(&huart2, rxBuffer, sizeof(rxBuffer));
}

// Отправка команды через очередь
void sendRS422Command(RS422Command* cmd) {
    uint8_t frameBuffer[32];
    int frameLength = 0;
    assembleFrame(slaveAddress, cmd->command, cmd->payload, cmd->payloadLength, frameBuffer, &frameLength);
    HAL_UART_Transmit_DMA(&huart2, frameBuffer, frameLength);
}

// Функции отправки команд
void rs422SendStatus(void) {
    if (isSending || isReceiving) return;
    isSending = true;

    RS422Command cmd = {.command = 'S', .payloadLength = 0};
    xQueueSend(rs422TxQueue, &cmd, portMAX_DELAY);
    isSending = false;
}

void rs422SendTransaction(FuelMode mode, uint32_t volume, uint32_t amount, uint16_t price) {
    if (isSending || isReceiving) return;
    if (price > 9999) {
        logMessage(LOG_LEVEL_ERROR, "Invalid price");
        displayMessage("Invalid price");
        return;
    }
    isSending = true;

    RS422Command cmd;
    cmd.command = (mode == FUEL_BY_VOLUME) ? 'V' : 'M';
    switch (mode) {
        case FUEL_BY_VOLUME:
            snprintf((char*)cmd.payload, sizeof(cmd.payload), "1;%06lu;%04u", volume, price);
            break;
        case FUEL_BY_PRICE:
            snprintf((char*)cmd.payload, sizeof(cmd.payload), "1;%06lu;%04u", amount, price);
            logMessage(LOG_LEVEL_DEBUG, "Sending transaction amount");
            break;
        case FUEL_BY_FULL_TANK:
            snprintf((char*)cmd.payload, sizeof(cmd.payload), "1;999999;%04u", price);
            break;
    }
    cmd.payloadLength = strlen((char*)cmd.payload);
    xQueueSend(rs422TxQueue, &cmd, portMAX_DELAY);
    isSending = false;
}

void rs422SendTransactionUpdate(void) {
    if (isSending || isReceiving) return;
    isSending = true;

    RS422Command cmd = {.command = 'T', .payloadLength = 0};
    xQueueSend(rs422TxQueue, &cmd, portMAX_DELAY);
    osDelay(1); // Задержка 500 мкс заменена на 1 мс в FreeRTOS
    isSending = false;
}

void rs422SendNozzleOff(void) {
    if (isSending || isReceiving) return;
    isSending = true;

    RS422Command cmd = {.command = 'N', .payloadLength = 0};
    xQueueSend(rs422TxQueue, &cmd, portMAX_DELAY);
    isSending = false;
}

void rs422SendLitersMonitor(void) {
    if (isSending || isReceiving) return;
    isSending = true;

    RS422Command cmd = {.command = 'L', .payloadLength = 0};
    xQueueSend(rs422TxQueue, &cmd, portMAX_DELAY);
    isSending = false;
}

void rs422SendRevenueStatus(void) {
    if (isSending || isReceiving) return;
    isSending = true;

    RS422Command cmd = {.command = 'R', .payloadLength = 0};
    xQueueSend(rs422TxQueue, &cmd, portMAX_DELAY);
    isSending = false;
}

void rs422SendTotalCounter(void) {
    if (isSending || isReceiving) return;
    isSending = true;

    RS422Command cmd = {.command = 'C', .payloadLength = 1};
    cmd.payload[0] = '1';
    xQueueSend(rs422TxQueue, &cmd, portMAX_DELAY);
    logMessage(LOG_LEVEL_DEBUG, "Sending C1 command");
    isSending = false;
}

void rs422SendPause(void) {
    if (isSending || isReceiving) return;
    isSending = true;

    RS422Command cmd = {.command = 'B', .payloadLength = 0};
    xQueueSend(rs422TxQueue, &cmd, portMAX_DELAY);
    logMessage(LOG_LEVEL_DEBUG, "Sending pause command");
    isSending = false;
}

void rs422SendResume(void) {
    if (isSending || isReceiving) return;
    isSending = true;

    RS422Command cmd = {.command = 'G', .payloadLength = 0};
    xQueueSend(rs422TxQueue, &cmd, portMAX_DELAY);
    logMessage(LOG_LEVEL_DEBUG, "Sending resume command");
    isSending = false;
}

// Ожидание ответа (асинхронно через очередь)
int rs422WaitForResponse(uint8_t* buffer, int expectedLength, char expectedCommand) {
    if (isReceiving) return 0;
    isReceiving = true;

    int count = 0;
    uint8_t rxData[32];
    TickType_t startTime = xTaskGetTickCount();

    while ((xTaskGetTickCount() - startTime) * portTICK_PERIOD_MS < RESPONSE_TIMEOUT) {
        if (xQueueReceive(rs422RxQueue, rxData, 10 / portTICK_PERIOD_MS) == pdTRUE) {
            memcpy(buffer, rxData, expectedLength);
            count = expectedLength;

            // Проверка формата ответа
            if (buffer[0] != 0x02 || buffer[1] != slaveAddress[0] || buffer[2] != slaveAddress[1] || buffer[3] != expectedCommand) {
                logMessage(LOG_LEVEL_ERROR, "Invalid response format or command");
                displayMessage("Invalid response from pump");
                count = -1;
                break;
            }

            // Проверка CRC
            uint8_t calcCRC = calculateCRC(buffer, expectedLength - 1);
            if (calcCRC != buffer[expectedLength - 1]) {
                logMessage(LOG_LEVEL_ERROR, "CRC mismatch");
                displayMessage("Invalid response from pump");
                count = -1;
            }
            break;
        }
    }

    isReceiving = false;
    return count;
}

// Callback для приёма данных через DMA
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart == &huart2) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xQueueSendFromISR(rs422RxQueue, rxBuffer, &xHigherPriorityTaskWoken);
        HAL_UART_Receive_DMA(&huart2, rxBuffer, sizeof(rxBuffer));
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}
