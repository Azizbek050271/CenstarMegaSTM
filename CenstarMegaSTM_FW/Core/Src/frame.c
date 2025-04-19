/* frame.c - Реализация формирования кадров GasKitLink v1.2 */

#include "frame.h"
#include "crc.h"
#include "config.h"

// Формирование кадра
void assembleFrame(const uint8_t* slaveAddress, char command, const uint8_t* payload, int payloadLength, uint8_t* frameBuffer, int* frameLength) {
    if (payloadLength > MAX_FRAME_PAYLOAD) return;
    int index = 0;
    frameBuffer[index++] = 0x02; // STX
    frameBuffer[index++] = slaveAddress[0];
    frameBuffer[index++] = slaveAddress[1];
    frameBuffer[index++] = (uint8_t)command;
    for (int i = 0; i < payloadLength; i++) {
        frameBuffer[index++] = payload[i];
    }
    uint8_t crc = calculateCRC(frameBuffer, index);
    frameBuffer[index++] = crc;
    *frameLength = index;
}
