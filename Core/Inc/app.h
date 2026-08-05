#ifndef APP_H
#define APP_H

#include "stm32f1xx_hal.h"

void App_Init(UART_HandleTypeDef *console_uart,
              UART_HandleTypeDef *bluetooth_uart,
              I2C_HandleTypeDef *display_bus,
              I2C_HandleTypeDef *motion_bus,
              SPI_HandleTypeDef *storage_bus);
void App_Process(void);

#endif
