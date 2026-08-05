#include "data_logger.h"

#include <string.h>
#include "w25q64.h"

#define LOG_REGION_ADDRESS 0x00000000UL
#define LOG_SECTOR_SIZE 4096U
#define LOG_SECTOR_COUNT 16U
#define LOG_REGION_SIZE (LOG_SECTOR_SIZE * LOG_SECTOR_COUNT)
#define LOG_RECORD_SIZE 32U
#define LOG_RECORDS_PER_SECTOR (LOG_SECTOR_SIZE / LOG_RECORD_SIZE)

static uint32_t next_address;
static uint32_t oldest_address;
static uint32_t next_sequence;
static uint16_t record_count;

static uint32_t AdvanceAddress(uint32_t address)
{
  address += LOG_RECORD_SIZE;
  if (address >= LOG_REGION_ADDRESS + LOG_REGION_SIZE)
  {
    address = LOG_REGION_ADDRESS;
  }
  return address;
}

static uint16_t Crc16Ccitt(const uint8_t *data, uint16_t length)
{
  uint16_t crc = 0xFFFFU;

  for (uint16_t index = 0U; index < length; index++)
  {
    crc ^= (uint16_t)data[index] << 8U;
    for (uint8_t bit = 0U; bit < 8U; bit++)
    {
      crc = (crc & 0x8000U) ? (uint16_t)((crc << 1U) ^ 0x1021U) :
                              (uint16_t)(crc << 1U);
    }
  }
  return crc;
}

static uint8_t IsErased(const uint8_t record[LOG_RECORD_SIZE])
{
  for (uint8_t index = 0U; index < LOG_RECORD_SIZE; index++)
  {
    if (record[index] != 0xFFU)
    {
      return 0U;
    }
  }
  return 1U;
}

static void PutUint16(uint8_t *destination, uint16_t value)
{
  destination[0] = (uint8_t)value;
  destination[1] = (uint8_t)(value >> 8U);
}

static void PutUint32(uint8_t *destination, uint32_t value)
{
  destination[0] = (uint8_t)value;
  destination[1] = (uint8_t)(value >> 8U);
  destination[2] = (uint8_t)(value >> 16U);
  destination[3] = (uint8_t)(value >> 24U);
}

static uint16_t GetUint16(const uint8_t *source)
{
  return (uint16_t)((uint16_t)source[0] | ((uint16_t)source[1] << 8U));
}

static uint32_t GetUint32(const uint8_t *source)
{
  return (uint32_t)source[0] | ((uint32_t)source[1] << 8U) |
         ((uint32_t)source[2] << 16U) | ((uint32_t)source[3] << 24U);
}

uint8_t DataLogger_TimestampIsValid(const DataLogTimestamp *timestamp)
{
  if ((timestamp == NULL) ||
      (timestamp->month < 1U) || (timestamp->month > 12U) ||
      (timestamp->day < 1U) || (timestamp->day > 31U) ||
      (timestamp->hour > 23U) ||
      (timestamp->minute > 59U) ||
      (timestamp->second > 59U))
  {
    return 0U;
  }
  return 1U;
}

static uint8_t IsValidRecord(const uint8_t record[LOG_RECORD_SIZE])
{
  uint16_t stored_crc;

  if ((record[0] != 'T') || (record[1] != 'L') ||
      (record[2] != 'G') || (record[3] != '1') ||
      (record[4] != 1U) ||
      (record[5] < (uint8_t)LOG_RECORD_BOOT) ||
      (record[5] > (uint8_t)LOG_RECORD_SHOCK) ||
      (record[6] != LOG_RECORD_SIZE))
  {
    return 0U;
  }
  stored_crc = GetUint16(&record[30]);
  return (stored_crc == Crc16Ccitt(record, 30U)) ? 1U : 0U;
}

HAL_StatusTypeDef DataLogger_Init(DataLoggerSummary *summary)
{
  uint8_t record[LOG_RECORD_SIZE];
  uint32_t address = LOG_REGION_ADDRESS;
  uint32_t minimum_sequence = 0xFFFFFFFFUL;
  uint32_t maximum_sequence = 0U;
  uint32_t maximum_address = LOG_REGION_ADDRESS;

  if (summary == NULL)
  {
    return HAL_ERROR;
  }

  summary->total_records = 0U;
  summary->motion_events = 0U;
  summary->shock_events = 0U;
  next_address = LOG_REGION_ADDRESS;
  oldest_address = LOG_REGION_ADDRESS;
  next_sequence = 1U;
  record_count = 0U;
  while (address < LOG_REGION_ADDRESS + LOG_REGION_SIZE)
  {
    uint32_t sequence;

    if (W25Q64_Read(address, record, sizeof(record)) != HAL_OK)
    {
      return HAL_ERROR;
    }
    if (IsErased(record) != 0U)
    {
      address += LOG_RECORD_SIZE;
      continue;
    }
    if (IsValidRecord(record) == 0U)
    {
      return HAL_ERROR;
    }

    if (record[5] == (uint8_t)LOG_RECORD_MOTION_START)
    {
      summary->motion_events++;
    }
    else if (record[5] == (uint8_t)LOG_RECORD_SHOCK)
    {
      summary->shock_events++;
    }
    sequence = GetUint32(&record[8]);
    if (sequence < minimum_sequence)
    {
      minimum_sequence = sequence;
      oldest_address = address;
    }
    if (sequence >= maximum_sequence)
    {
      maximum_sequence = sequence;
      maximum_address = address;
    }
    record_count++;
    address += LOG_RECORD_SIZE;
  }

  if (record_count != 0U)
  {
    next_sequence = maximum_sequence + 1U;
    next_address = AdvanceAddress(maximum_address);
  }
  summary->total_records = record_count;
  return HAL_OK;
}

HAL_StatusTypeDef DataLogger_Append(LogRecordType type,
                                    uint32_t uptime_seconds,
                                    int16_t temperature_tenths,
                                    int16_t x_mg, int16_t y_mg,
                                    int16_t z_mg,
                                    const DataLogTimestamp *timestamp)
{
  uint8_t record[LOG_RECORD_SIZE];
  uint8_t readback[LOG_RECORD_SIZE];
  uint8_t existing_record[LOG_RECORD_SIZE];
  uint16_t crc;

  if ((type < LOG_RECORD_BOOT) || (type > LOG_RECORD_SHOCK))
  {
    return HAL_ERROR;
  }

  if (W25Q64_Read(next_address, existing_record,
                  sizeof(existing_record)) != HAL_OK)
  {
    return HAL_ERROR;
  }
  if (IsErased(existing_record) == 0U)
  {
    uint32_t sector_address = next_address & 0x00FFF000UL;
    uint16_t removed_records = (record_count < LOG_RECORDS_PER_SECTOR) ?
                               record_count : LOG_RECORDS_PER_SECTOR;

    if ((next_address & (LOG_SECTOR_SIZE - 1U)) != 0U)
    {
      return HAL_ERROR;
    }
    if (W25Q64_EraseSector(sector_address) != HAL_OK)
    {
      return HAL_ERROR;
    }
    record_count -= removed_records;
    oldest_address = AdvanceAddress(sector_address + LOG_SECTOR_SIZE -
                                    LOG_RECORD_SIZE);
  }

  memset(record, 0U, sizeof(record));
  record[0] = 'T';
  record[1] = 'L';
  record[2] = 'G';
  record[3] = '1';
  record[4] = 1U;
  record[5] = (uint8_t)type;
  record[6] = LOG_RECORD_SIZE;
  PutUint32(&record[8], next_sequence);
  PutUint32(&record[12], uptime_seconds);
  PutUint16(&record[16], (uint16_t)temperature_tenths);
  PutUint16(&record[18], (uint16_t)x_mg);
  PutUint16(&record[20], (uint16_t)y_mg);
  PutUint16(&record[22], (uint16_t)z_mg);
  if (DataLogger_TimestampIsValid(timestamp) != 0U)
  {
    record[24] = timestamp->year_from_2000;
    record[25] = timestamp->month;
    record[26] = timestamp->day;
    record[27] = timestamp->hour;
    record[28] = timestamp->minute;
    record[29] = timestamp->second;
  }
  crc = Crc16Ccitt(record, 30U);
  PutUint16(&record[30], crc);

  if ((W25Q64_ProgramPage(next_address, record, sizeof(record)) != HAL_OK) ||
      (W25Q64_Read(next_address, readback, sizeof(readback)) != HAL_OK) ||
      (memcmp(record, readback, sizeof(record)) != 0))
  {
    return HAL_ERROR;
  }

  if (record_count == 0U)
  {
    oldest_address = next_address;
  }
  next_address = AdvanceAddress(next_address);
  next_sequence++;
  record_count++;
  return HAL_OK;
}

HAL_StatusTypeDef DataLogger_Clear(void)
{
  for (uint16_t sector = 0U; sector < LOG_SECTOR_COUNT; sector++)
  {
    uint32_t address = LOG_REGION_ADDRESS +
                       ((uint32_t)sector * LOG_SECTOR_SIZE);

    if (W25Q64_EraseSector(address) != HAL_OK)
    {
      return HAL_ERROR;
    }
  }

  next_address = LOG_REGION_ADDRESS;
  oldest_address = LOG_REGION_ADDRESS;
  next_sequence = 1U;
  record_count = 0U;
  return HAL_OK;
}

uint16_t DataLogger_GetRecordCount(void)
{
  return record_count;
}

HAL_StatusTypeDef DataLogger_Read(uint16_t index, DataLogRecord *record)
{
  uint8_t raw_record[LOG_RECORD_SIZE];
  uint32_t address;

  if ((record == NULL) || (index >= DataLogger_GetRecordCount()))
  {
    return HAL_ERROR;
  }

  address = oldest_address + ((uint32_t)index * LOG_RECORD_SIZE);
  if (address >= LOG_REGION_ADDRESS + LOG_REGION_SIZE)
  {
    address -= LOG_REGION_SIZE;
  }
  if ((W25Q64_Read(address, raw_record, sizeof(raw_record)) != HAL_OK) ||
      (IsValidRecord(raw_record) == 0U))
  {
    return HAL_ERROR;
  }

  record->type = (LogRecordType)raw_record[5];
  record->sequence = GetUint32(&raw_record[8]);
  record->uptime_seconds = GetUint32(&raw_record[12]);
  record->temperature_tenths = (int16_t)GetUint16(&raw_record[16]);
  record->x_mg = (int16_t)GetUint16(&raw_record[18]);
  record->y_mg = (int16_t)GetUint16(&raw_record[20]);
  record->z_mg = (int16_t)GetUint16(&raw_record[22]);
  record->timestamp.year_from_2000 = raw_record[24];
  record->timestamp.month = raw_record[25];
  record->timestamp.day = raw_record[26];
  record->timestamp.hour = raw_record[27];
  record->timestamp.minute = raw_record[28];
  record->timestamp.second = raw_record[29];
  return HAL_OK;
}
