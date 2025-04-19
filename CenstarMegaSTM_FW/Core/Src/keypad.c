/* keypad.c - Реализация сканирования клавиатуры 5×4 */

#include "keypad.h"
#include "config.h"
#include <cmsis_os.h>

// Карта клавиш 5×4 (как в вашем тестовом коде)
static const char KeyMap[KEYPAD_ROW_COUNT][KEYPAD_COL_COUNT] = {
    {'A', 'F', 'G', 'H'},
    {'B', '1', '2', '3'},
    {'C', '4', '5', '6'},
    {'D', '7', '8', '9'},
    {'E', '*', '0', 'K'}
};

// Строки (выходы)
static GPIO_TypeDef* RowPort[KEYPAD_ROW_COUNT] = {ROW1_PORT, ROW2_PORT, ROW3_PORT, ROW4_PORT, ROW5_PORT};
static const uint16_t RowPin[KEYPAD_ROW_COUNT] = {ROW1_PIN, ROW2_PIN, ROW3_PIN, ROW4_PIN, ROW5_PIN};

// Столбцы (входы)
static GPIO_TypeDef* ColPort[KEYPAD_COL_COUNT] = {COL1_PORT, COL2_PORT, COL3_PORT, COL4_PORT};
static const uint16_t ColPin[KEYPAD_COL_COUNT] = {COL1_PIN, COL2_PIN, COL3_PIN, COL4_PIN};

// Функция сканирования клавиатуры
char getKeypadKey(void) {
    for (uint8_t r = 0; r < KEYPAD_ROW_COUNT; r++) {
        // Активируем одну строку (LOW), остальные HIGH
        for (uint8_t i = 0; i < KEYPAD_ROW_COUNT; i++) {
            HAL_GPIO_WritePin(RowPort[i], RowPin[i], (i == r) ? GPIO_PIN_RESET : GPIO_PIN_SET);
        }

        osDelay(1); // Задержка для стабилизации

        for (uint8_t c = 0; c < KEYPAD_COL_COUNT; c++) {
            if (HAL_GPIO_ReadPin(ColPort[c], ColPin[c]) == GPIO_PIN_RESET) {
                osDelay(KEY_DEBOUNCE_MS); // Антидребезг
                if (HAL_GPIO_ReadPin(ColPort[c], ColPin[c]) == GPIO_PIN_RESET) {
                    return KeyMap[r][c];
                }
            }
        }
    }
    return 0; // Ничего не нажато
}
