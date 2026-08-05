#ifndef ADXL345_H
#define ADXL345_H

#include "stm32f1xx_hal.h"

HAL_StatusTypeDef ADXL345_Init(I2C_HandleTypeDef *i2c, uint8_t address);
HAL_StatusTypeDef ADXL345_ReadMillig(int16_t *x_mg, int16_t *y_mg,
                                    int16_t *z_mg);

#endif
