/* utils.h - Заголовочный файл для утилит */

#ifndef UTILS_H
#define UTILS_H

#include "stm32f4xx_hal.h"
#include <stdio.h>

// Преобразование числа в строку с ведущими нулями
void intToString(uint16_t value, uint8_t digits, char* buffer);

#endif /* UTILS_H */
