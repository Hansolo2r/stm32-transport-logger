#ifndef DS18B20_H
#define DS18B20_H

#include "stm32f1xx_hal.h"

void DS18B20_Init(GPIO_TypeDef *port, uint16_t pin);
HAL_StatusTypeDef DS18B20_StartConversion(void);
HAL_StatusTypeDef DS18B20_ReadTemperatureRaw(int16_t *raw_temperature);

#endif
