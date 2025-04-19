/* keypad.h - Заголовочный файл для обработки клавиатуры 5×4 */

#ifndef KEYPAD_H
#define KEYPAD_H

#include "stm32f4xx_hal.h"

// Функция сканирования клавиатуры, возвращает символ или 0
char getKeypadKey(void);

#endif /* KEYPAD_H */
