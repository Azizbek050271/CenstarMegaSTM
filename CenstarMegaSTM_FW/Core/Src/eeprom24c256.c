#include "eeprom24c256.h"
#include "main.h" /* For hi2c1 and huart2 */

extern I2C_HandleTypeDef hi2c1;
extern UART_HandleTypeDef huart2;

/* Wait for internal write cycle completion */
static HAL_StatusTypeDef EEPROM_WaitReady(void)
{
    uint32_t tick = HAL_GetTick();
    while (HAL_I2C_IsDeviceReady(&hi2c1, EEPROM_I2C_ADDR, 1, 10) != HAL_OK)
    {
        if (HAL_GetTick() - tick > 25)
        {
            HAL_UART_Transmit(&huart2, (uint8_t*)"EEPROM Timeout\r\n", 16, HAL_MAX_DELAY);
            return HAL_TIMEOUT;
        }
    }
    HAL_UART_Transmit(&huart2, (uint8_t*)"EEPROM Ready\r\n", 14, HAL_MAX_DELAY);
    return HAL_OK;
}

/* Write arbitrary number of bytes with automatic page splitting */
HAL_StatusTypeDef EEPROM_Write(uint16_t memAddr, const uint8_t *data, uint16_t len)
{
    /* Check if EEPROM is ready */
    if (HAL_I2C_IsDeviceReady(&hi2c1, EEPROM_I2C_ADDR, 2, 10) != HAL_OK)
    {
        HAL_UART_Transmit(&huart2, (uint8_t*)"EEPROM Not Found\r\n", 18, HAL_MAX_DELAY);
        return HAL_ERROR;
    }

    /* Store I2C configuration for reset */
    I2C_InitTypeDef i2c_config = {
        .ClockSpeed = 100000,
        .DutyCycle = I2C_DUTYCYCLE_2,
        .OwnAddress1 = 0,
        .AddressingMode = I2C_ADDRESSINGMODE_7BIT,
        .DualAddressMode = I2C_DUALADDRESS_DISABLE,
        .OwnAddress2 = 0,
        .GeneralCallMode = I2C_GENERALCALL_DISABLE,
        .NoStretchMode = I2C_NOSTRETCH_DISABLE
    };

    while (len)
    {
        uint16_t pageRemain = EEPROM_PAGE_SIZE - (memAddr % EEPROM_PAGE_SIZE);
        uint16_t chunk = (len < pageRemain) ? len : pageRemain;

        HAL_UART_Transmit(&huart2, (uint8_t*)"Writing chunk...\r\n", 18, HAL_MAX_DELAY);
        if (HAL_I2C_Mem_Write(&hi2c1, EEPROM_I2C_ADDR, memAddr, I2C_MEMADD_SIZE_16BIT,
                              (uint8_t*)data, chunk, 100) != HAL_OK)
        {
            HAL_UART_Transmit(&huart2, (uint8_t*)"EEPROM Write Error\r\n", 20, HAL_MAX_DELAY);
            /* Reset I2C */
            HAL_I2C_DeInit(&hi2c1);
            hi2c1.Instance = I2C1;
            hi2c1.Init = i2c_config;
            if (HAL_I2C_Init(&hi2c1) != HAL_OK)
            {
                HAL_UART_Transmit(&huart2, (uint8_t*)"I2C Reinit Error\r\n", 19, HAL_MAX_DELAY);
                return HAL_ERROR;
            }
            return HAL_ERROR;
        }

        if (EEPROM_WaitReady() != HAL_OK)
        {
            HAL_UART_Transmit(&huart2, (uint8_t*)"EEPROM Wait Error\r\n", 19, HAL_MAX_DELAY);
            /* Reset I2C */
            HAL_I2C_DeInit(&hi2c1);
            hi2c1.Instance = I2C1;
            hi2c1.Init = i2c_config;
            if (HAL_I2C_Init(&hi2c1) != HAL_OK)
            {
                HAL_UART_Transmit(&huart2, (uint8_t*)"I2C Reinit Error\r\n", 19, HAL_MAX_DELAY);
                return HAL_ERROR;
            }
            return HAL_ERROR;
        }

        memAddr += chunk;
        data += chunk;
        len -= chunk;
    }
    HAL_UART_Transmit(&huart2, (uint8_t*)"EEPROM Write OK\r\n", 17, HAL_MAX_DELAY);
    return HAL_OK;
}

/* Read len bytes, 16-bit address */
HAL_StatusTypeDef EEPROM_Read(uint16_t memAddr, uint8_t *data, uint16_t len)
{
    /* Check if EEPROM is ready */
    if (HAL_I2C_IsDeviceReady(&hi2c1, EEPROM_I2C_ADDR, 2, 10) != HAL_OK)
    {
        HAL_UART_Transmit(&huart2, (uint8_t*)"EEPROM Not Found\r\n", 18, HAL_MAX_DELAY);
        return HAL_ERROR;
    }

    /* Store I2C configuration for reset */
    I2C_InitTypeDef i2c_config = {
        .ClockSpeed = 100000,
        .DutyCycle = I2C_DUTYCYCLE_2,
        .OwnAddress1 = 0,
        .AddressingMode = I2C_ADDRESSINGMODE_7BIT,
        .DualAddressMode = I2C_DUALADDRESS_DISABLE,
        .OwnAddress2 = 0,
        .GeneralCallMode = I2C_GENERALCALL_DISABLE,
        .NoStretchMode = I2C_NOSTRETCH_DISABLE
    };

    HAL_StatusTypeDef status = HAL_I2C_Mem_Read(&hi2c1, EEPROM_I2C_ADDR, memAddr,
                                                I2C_MEMADD_SIZE_16BIT, data, len, 100);
    if (status != HAL_OK)
    {
        HAL_UART_Transmit(&huart2, (uint8_t*)"EEPROM Read Error\r\n", 19, HAL_MAX_DELAY);
        /* Reset I2C */
        HAL_I2C_DeInit(&hi2c1);
        hi2c1.Instance = I2C1;
        hi2c1.Init = i2c_config;
        if (HAL_I2C_Init(&hi2c1) != HAL_OK)
        {
            HAL_UART_Transmit(&huart2, (uint8_t*)"I2C Reinit Error\r\n", 19, HAL_MAX_DELAY);
            return HAL_ERROR;
        }
        return HAL_ERROR;
    }
    HAL_UART_Transmit(&huart2, (uint8_t*)"EEPROM Read OK\r\n", 16, HAL_MAX_DELAY);
    return HAL_OK;
}
