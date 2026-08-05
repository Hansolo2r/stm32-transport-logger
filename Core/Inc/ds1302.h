#ifndef DS1302_H
#define DS1302_H

#include "stm32f1xx_hal.h"

typedef struct
{
  uint16_t year;
  uint8_t month;
  uint8_t day;
  uint8_t weekday;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
} DS1302_DateTime;

void DS1302_Init(GPIO_TypeDef *clk_port, uint16_t clk_pin,
                 GPIO_TypeDef *dat_port, uint16_t dat_pin,
                 GPIO_TypeDef *rst_port, uint16_t rst_pin);
HAL_StatusTypeDef DS1302_ReadDateTime(DS1302_DateTime *date_time);
HAL_StatusTypeDef DS1302_WriteDateTime(const DS1302_DateTime *date_time);
uint8_t DS1302_DateTimeIsValid(const DS1302_DateTime *date_time);

#endif
