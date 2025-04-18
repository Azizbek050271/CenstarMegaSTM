#include "keypad.h"

/* карта клавиш 5×4 */
static const char KeyMap[5][4] = {
  {'A','F','G','H'},
  {'B','1','2','3'},
  {'C','4','5','6'},
  {'D','7','8','9'},
  {'E','*','0','K'}
};

/* строки */
static GPIO_TypeDef* RowPort[5]={GPIOC,GPIOC,GPIOC,GPIOC,GPIOC};
static const uint16_t RowPin[5]={
    GPIO_PIN_0,GPIO_PIN_1,GPIO_PIN_2,GPIO_PIN_3,GPIO_PIN_4};

/* колонки */
static GPIO_TypeDef* ColPort[4]={GPIOB,GPIOB,GPIOB,GPIOB};
static const uint16_t ColPin[4]={
    GPIO_PIN_0,GPIO_PIN_1,GPIO_PIN_11,GPIO_PIN_12};

/* вернёт символ или 0 */
char KEYPAD_Scan(void)
{
    for (uint8_t r=0;r<5;r++)
    {
        /* активируем одну строку (LOW), остальные HIGH */
        for (uint8_t i=0;i<5;i++)
            HAL_GPIO_WritePin(RowPort[i],RowPin[i],
                              (i==r)?GPIO_PIN_RESET:GPIO_PIN_SET);

        HAL_Delay(1); /* стабилизация */

        for (uint8_t c=0;c<4;c++)
        {
            if (HAL_GPIO_ReadPin(ColPort[c],ColPin[c])==GPIO_PIN_RESET)
            {
                HAL_Delay(15);                      /* антидребезг */
                if (HAL_GPIO_ReadPin(ColPort[c],ColPin[c])==GPIO_PIN_RESET)
                    return KeyMap[r][c];
            }
        }
    }
    return 0;
}
