#ifndef EEPROM24C256_H
#define EEPROM24C256_H

#include "stm32f4xx_hal.h"

#define EEPROM_I2C_ADDR   (0x50 << 1)   /* 0x50 on bus → HAL address (0xA0) */
#define EEPROM_PAGE_SIZE  64            /* Bytes per page */
#define EEPROM_SIZE       32768         /* 32 KB */

HAL_StatusTypeDef EEPROM_Write(uint16_t memAddr, const uint8_t *data, uint16_t len);
HAL_StatusTypeDef EEPROM_Read(uint16_t memAddr, uint8_t *data, uint16_t len);

#endif /* EEPROM24C256_H */
