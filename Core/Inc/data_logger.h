#ifndef DATA_LOGGER_H
#define DATA_LOGGER_H

#include "stm32f1xx_hal.h"

typedef enum
{
  LOG_RECORD_BOOT = 1U,
  LOG_RECORD_TEMPERATURE = 2U,
  LOG_RECORD_MOTION_START = 3U,
  LOG_RECORD_MOTION_END = 4U,
  LOG_RECORD_SHOCK = 5U
} LogRecordType;

typedef struct
{
  uint16_t total_records;
  uint16_t motion_events;
  uint16_t shock_events;
} DataLoggerSummary;

typedef struct
{
  uint8_t year_from_2000;
  uint8_t month;
  uint8_t day;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
} DataLogTimestamp;

typedef struct
{
  LogRecordType type;
  uint32_t sequence;
  uint32_t uptime_seconds;
  int16_t temperature_tenths;
  int16_t x_mg;
  int16_t y_mg;
  int16_t z_mg;
  DataLogTimestamp timestamp;
} DataLogRecord;

HAL_StatusTypeDef DataLogger_Init(DataLoggerSummary *summary);
HAL_StatusTypeDef DataLogger_Append(LogRecordType type,
                                    uint32_t uptime_seconds,
                                    int16_t temperature_tenths,
                                    int16_t x_mg, int16_t y_mg,
                                    int16_t z_mg,
                                    const DataLogTimestamp *timestamp);
HAL_StatusTypeDef DataLogger_Clear(void);
uint16_t DataLogger_GetRecordCount(void);
HAL_StatusTypeDef DataLogger_Read(uint16_t index, DataLogRecord *record);
uint8_t DataLogger_TimestampIsValid(const DataLogTimestamp *timestamp);

#endif
