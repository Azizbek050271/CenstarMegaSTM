/* frame.h - Заголовочный файл для формирования кадров GasKitLink v1.2 */

#ifndef FRAME_H
#define FRAME_H

#include "stm32f4xx_hal.h"

// Формирование кадра
void assembleFrame(const uint8_t* slaveAddress, char command, const uint8_t* payload, int payloadLength, uint8_t* frameBuffer, int* frameLength);

#endif /* FRAME_H */
