#ifndef SSD1306_H
#define SSD1306_H

#include "stm32f1xx_hal.h"

HAL_StatusTypeDef SSD1306_Init(I2C_HandleTypeDef *i2c, uint8_t address);

typedef enum
{
  SSD1306_ALERT_NONE = 0U,
  SSD1306_ALERT_MOTION,
  SSD1306_ALERT_SHOCK
} SSD1306_Alert;

HAL_StatusTypeDef SSD1306_ShowLoggerStatus(const char *temperature_text,
                                           uint16_t shock_count,
                                           uint16_t motion_count,
                                           SSD1306_Alert alert,
                                           uint8_t figure_frame);
HAL_StatusTypeDef SSD1306_SetDisplayEnabled(uint8_t enabled);
HAL_StatusTypeDef SSD1306_UpdateFigure(SSD1306_Alert alert,
                                       uint8_t figure_frame);

#endif
