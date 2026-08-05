#ifndef W25Q64_H
#define W25Q64_H

#include "stm32f1xx_hal.h"

HAL_StatusTypeDef W25Q64_ReadJedecId(SPI_HandleTypeDef *spi,
                                    GPIO_TypeDef *cs_port, uint16_t cs_pin,
                                    uint8_t jedec_id[3]);
uint8_t W25Q64_IsExpectedDevice(const uint8_t jedec_id[3]);
HAL_StatusTypeDef W25Q64_Read(uint32_t address, uint8_t *data,
                             uint16_t length);
HAL_StatusTypeDef W25Q64_EraseSector(uint32_t address);
HAL_StatusTypeDef W25Q64_ProgramPage(uint32_t address, const uint8_t *data,
                                    uint16_t length);

#endif
