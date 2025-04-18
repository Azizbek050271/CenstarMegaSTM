/* keypad.h  –  простейший интерфейс клавиатуры 5×4 */
#ifndef KEYPAD_H
#define KEYPAD_H

#include "stm32f4xx_hal.h"

/* Возвращает символ карты или 0, если ничего не нажато */
char KEYPAD_Scan(void);

#endif /* KEYPAD_H */
