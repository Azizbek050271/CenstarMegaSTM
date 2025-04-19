/* rs422.h - Заголовочный файл для коммуникации по RS-422 */

#ifndef RS422_H
#define RS422_H

#include "stm32f4xx_hal.h"
#include "fsm.h"
#include <stdbool.h>

// Структура команды RS-422
typedef struct {
    char command;
    uint8_t payload[16];
    int payloadLength;
} RS422Command;

// Инициализация RS-422
void initRS422(void);

// Функции отправки команд
void rs422SendStatus(void);
void rs422SendTransaction(FuelMode mode, uint32_t volume, uint32_t amount, uint16_t price);
void rs422SendTransactionUpdate(void);
void rs422SendNozzleOff(void);
void rs422SendLitersMonitor(void);
void rs422SendRevenueStatus(void);
void rs422SendTotalCounter(void);
void rs422SendPause(void);
void rs422SendResume(void);

// Функция ожидания ответа
int rs422WaitForResponse(uint8_t* buffer, int expectedLength, char expectedCommand);

// Отправка команды (внутренняя функция)
void sendRS422Command(RS422Command* cmd);

// Логирование (определено в fsm.c)
void logMessage(int level, const char* msg);

#endif /* RS422_H */
