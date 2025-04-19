/* crc.c - Реализация расчёта CRC */

#include "crc.h"

// Расчёт CRC для буфера данных
uint8_t calculateCRC(const uint8_t* data, int length) {
    if (length < 2) return 0;
    uint8_t crc = data[1];
    for (int i = 2; i < length; i++) {
        crc ^= data[i];
    }
    return crc;
}
