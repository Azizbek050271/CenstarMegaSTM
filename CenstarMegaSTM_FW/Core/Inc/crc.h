/* crc.h - Заголовочный файл для расчёта CRC */

#ifndef CRC_H
#define CRC_H

#include "stm32f4xx_hal.h"

// Функция расчёта CRC
uint8_t calculateCRC(const uint8_t* data, int length);

#endif /* CRC_H */
