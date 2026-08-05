#include "app.h"

#include <string.h>
#include "adxl345.h"
#include "data_logger.h"
#include "ds1302.h"
#include "ds18b20.h"
#include "main.h"
#include "ssd1306.h"
#include "w25q64.h"

#define ACCEL_SAMPLE_INTERVAL_MS 10U
#define ACCEL_REPORT_INTERVAL_MS 1000U
#define ADXL_RECOVERY_INTERVAL_MS 1000U
#define ADXL_FAILURE_LIMIT 5U
#define CONFIG_ADDRESS 0x007FE000UL
#define CONFIG_DATA_SIZE 32U
#define CONFIG_SHOCK_MIN_MG 1100U
#define CONFIG_SHOCK_MAX_MG 8000U
#define CONFIG_MOTION_MIN_MG 50U
#define CONFIG_MOTION_MAX_MG 800U
#define CONFIG_MOTION_CONFIRM_MIN_MS 10U
#define CONFIG_MOTION_CONFIRM_MAX_MS 2000U
#define CONFIG_STILL_CONFIRM_MIN_MS 100U
#define CONFIG_STILL_CONFIRM_MAX_MS 30000U
#define CONFIG_COOLDOWN_MIN_MS 100U
#define CONFIG_COOLDOWN_MAX_MS 10000U
#define CONFIG_TEMP_INTERVAL_MIN_S 5U
#define CONFIG_TEMP_INTERVAL_MAX_S 3600U
#define FLASH_TEST_ADDRESS 0x007FF000UL
#define OLED_ALERT_DURATION_MS 2000U
#define OLED_ANIMATION_INTERVAL_MS 500U
#define OLED_POWER_ON_DELAY_MS 200U
#define OLED_INIT_RETRY_DELAY_MS 200U
#define UART_RX_BUFFER_SIZE 64U
#define COMMAND_BUFFER_SIZE 40U

typedef struct
{
  uint16_t shock_threshold_mg;
  uint16_t motion_delta_mg;
  uint16_t motion_confirm_ms;
  uint16_t still_confirm_ms;
  uint16_t shock_cooldown_ms;
  uint16_t temperature_log_interval_s;
} AppConfiguration;

typedef struct
{
  UART_HandleTypeDef *uart;
  volatile uint8_t rx_head;
  volatile uint8_t rx_tail;
  volatile uint8_t rx_overflow;
  uint8_t rx_byte;
  uint8_t rx_buffer[UART_RX_BUFFER_SIZE];
  char command_buffer[COMMAND_BUFFER_SIZE];
  uint8_t command_length;
} AppUartChannel;

static UART_HandleTypeDef *debug_uart;
static UART_HandleTypeDef *ble_uart;
static UART_HandleTypeDef *active_output_uart;
static AppUartChannel console_channel;
static AppUartChannel bluetooth_channel;
static I2C_HandleTypeDef *display_i2c;
static I2C_HandleTypeDef *motion_i2c;
static uint32_t led_updated_at;
static uint32_t temperature_started_at;
static uint32_t temperature_next_at;
static uint8_t conversion_pending;
static uint8_t oled_ready;
static uint8_t adxl_ready;
static uint8_t flash_log_ready;
static uint8_t flash_read_ready;
static uint8_t flash_configuration_ready;
static uint8_t rtc_ready;
static uint8_t temperature_valid;
static uint8_t adxl_i2c_address;
static uint8_t adxl_read_failures;
static uint8_t oled_refresh_pending;
static uint8_t oled_figure_frame;
static uint8_t logging_paused;
static uint8_t motion_active;
static uint16_t motion_candidate_samples;
static uint16_t still_samples;
static uint16_t motion_event_count;
static uint16_t shock_event_count;
static int16_t latest_temperature_tenths;
static int16_t latest_x_mg;
static int16_t latest_y_mg;
static int16_t latest_z_mg;
static uint32_t acceleration_sampled_at;
static uint32_t acceleration_reported_at;
static uint32_t shock_reported_at;
static uint32_t adxl_recovery_at;
static uint32_t temperature_log_next_at;
static uint32_t oled_alert_until;
static uint32_t oled_animated_at;
static uint32_t rtc_sampled_at;
static SSD1306_Alert oled_alert;
static char latest_temperature_text[9];
static DS1302_DateTime latest_date_time;
static AppConfiguration configuration;

static char HexDigit(uint8_t value)
{
  return (value < 10U) ? (char)('0' + value) :
                         (char)('A' + (value - 10U));
}

static void SendStatus(const char *text)
{
  HAL_UART_Transmit(active_output_uart, (const uint8_t *)text,
                    (uint16_t)strlen(text), HAL_MAX_DELAY);
}

static UART_HandleTypeDef *SetOutputUart(UART_HandleTypeDef *uart)
{
  UART_HandleTypeDef *previous = active_output_uart;

  active_output_uart = uart;
  return previous;
}

static void SendHexByte(uint8_t value)
{
  char text[3];

  text[0] = HexDigit((uint8_t)(value >> 4U));
  text[1] = HexDigit((uint8_t)(value & 0x0FU));
  text[2] = '\0';
  SendStatus(text);
}

static void SendSignedInteger(int16_t value)
{
  char text[8];
  uint8_t position = sizeof(text) - 1U;
  uint16_t magnitude;

  text[position] = '\0';
  if (value < 0)
  {
    magnitude = (uint16_t)(-value);
  }
  else
  {
    magnitude = (uint16_t)value;
  }

  do
  {
    text[--position] = (char)('0' + (magnitude % 10U));
    magnitude /= 10U;
  } while (magnitude != 0U);

  if (value < 0)
  {
    text[--position] = '-';
  }
  SendStatus(&text[position]);
}

static void SendUnsignedInteger(uint16_t value)
{
  char text[6];
  uint8_t position = sizeof(text) - 1U;

  text[position] = '\0';
  do
  {
    text[--position] = (char)('0' + (value % 10U));
    value /= 10U;
  } while (value != 0U);
  SendStatus(&text[position]);
}

static void SendUnsignedInteger32(uint32_t value)
{
  char text[11];
  uint8_t position = sizeof(text) - 1U;

  text[position] = '\0';
  do
  {
    text[--position] = (char)('0' + (value % 10U));
    value /= 10U;
  } while (value != 0U);
  SendStatus(&text[position]);
}

static void SendTwoDigits(uint8_t value)
{
  char text[3];

  text[0] = (char)('0' + (value / 10U));
  text[1] = (char)('0' + (value % 10U));
  text[2] = '\0';
  SendStatus(text);
}

static void SendDateTime(const DS1302_DateTime *date_time)
{
  SendUnsignedInteger(date_time->year);
  SendStatus("-");
  SendTwoDigits(date_time->month);
  SendStatus("-");
  SendTwoDigits(date_time->day);
  SendStatus(" ");
  SendTwoDigits(date_time->hour);
  SendStatus(":");
  SendTwoDigits(date_time->minute);
  SendStatus(":");
  SendTwoDigits(date_time->second);
}

static void SendLogTimestamp(const DataLogTimestamp *timestamp)
{
  if (DataLogger_TimestampIsValid(timestamp) == 0U)
  {
    SendStatus("NA");
    return;
  }
  SendUnsignedInteger((uint16_t)(2000U + timestamp->year_from_2000));
  SendStatus("-");
  SendTwoDigits(timestamp->month);
  SendStatus("-");
  SendTwoDigits(timestamp->day);
  SendStatus(" ");
  SendTwoDigits(timestamp->hour);
  SendStatus(":");
  SendTwoDigits(timestamp->minute);
  SendStatus(":");
  SendTwoDigits(timestamp->second);
}

static uint8_t ParseDigits(const char *text, uint8_t count, uint16_t *value)
{
  uint16_t result = 0U;

  for (uint8_t index = 0U; index < count; index++)
  {
    if ((text[index] < '0') || (text[index] > '9'))
    {
      return 0U;
    }
    result = (uint16_t)((result * 10U) + (uint16_t)(text[index] - '0'));
  }
  *value = result;
  return 1U;
}

static uint8_t CalculateWeekday(uint16_t year, uint8_t month, uint8_t day)
{
  static const uint16_t days_before_month[] =
    {0U, 31U, 59U, 90U, 120U, 151U, 181U, 212U, 243U, 273U, 304U, 334U};
  uint32_t days = 0U;

  for (uint16_t candidate = 2000U; candidate < year; candidate++)
  {
    days += ((candidate % 4U) == 0U) ? 366U : 365U;
  }
  days += days_before_month[month - 1U];
  if ((month > 2U) && ((year % 4U) == 0U))
  {
    days++;
  }
  days += (uint32_t)day - 1U;
  return (uint8_t)(((days + 5U) % 7U) + 1U);
}

static uint8_t ParseDateTimeCommand(const char *command,
                                    DS1302_DateTime *date_time)
{
  const char *text = command + 9U;
  uint16_t value;

  if ((strncmp(command, "time set ", 9U) != 0) ||
      (strlen(text) != 19U) ||
      (text[4] != '-') || (text[7] != '-') || (text[10] != ' ') ||
      (text[13] != ':') || (text[16] != ':') ||
      (ParseDigits(&text[0], 4U, &date_time->year) == 0U) ||
      (ParseDigits(&text[5], 2U, &value) == 0U))
  {
    return 0U;
  }
  date_time->month = (uint8_t)value;
  if (ParseDigits(&text[8], 2U, &value) == 0U)
  {
    return 0U;
  }
  date_time->day = (uint8_t)value;
  if (ParseDigits(&text[11], 2U, &value) == 0U)
  {
    return 0U;
  }
  date_time->hour = (uint8_t)value;
  if (ParseDigits(&text[14], 2U, &value) == 0U)
  {
    return 0U;
  }
  date_time->minute = (uint8_t)value;
  if (ParseDigits(&text[17], 2U, &value) == 0U)
  {
    return 0U;
  }
  date_time->second = (uint8_t)value;
  if ((date_time->year < 2000U) || (date_time->year > 2099U) ||
      (date_time->month < 1U) || (date_time->month > 12U))
  {
    return 0U;
  }
  date_time->weekday = CalculateWeekday(date_time->year,
                                        date_time->month,
                                        date_time->day);
  return DS1302_DateTimeIsValid(date_time);
}

static uint8_t UpdateRtcTime(void)
{
  rtc_sampled_at = HAL_GetTick();
  if (DS1302_ReadDateTime(&latest_date_time) == HAL_OK)
  {
    rtc_ready = 1U;
    return 1U;
  }
  rtc_ready = 0U;
  return 0U;
}

static void ReportRtcTime(void)
{
  if (UpdateRtcTime() != 0U)
  {
    SendStatus("DS1302时间：");
    SendDateTime(&latest_date_time);
    SendStatus("\r\n");
  }
  else
  {
    SendStatus("DS1302：读取值无效，请检查接线或使用 time set 设置时间\r\n");
  }
}

static void HandleTimeCommand(const char *command)
{
  DS1302_DateTime date_time;

  if (strcmp(command, "time") == 0)
  {
    ReportRtcTime();
    return;
  }
  if (ParseDateTimeCommand(command, &date_time) == 0U)
  {
    SendStatus("时间格式错误，示例：time set 2026-08-05 14:30:00\r\n");
    return;
  }
  if (DS1302_WriteDateTime(&date_time) != HAL_OK)
  {
    rtc_ready = 0U;
    SendStatus("DS1302：时间写入失败\r\n");
    return;
  }
  SendStatus("DS1302：时间设置完成\r\n");
  ReportRtcTime();
}

static void PutUint16(uint8_t *data, uint16_t value)
{
  data[0] = (uint8_t)value;
  data[1] = (uint8_t)(value >> 8U);
}

static uint16_t GetUint16(const uint8_t *data)
{
  return (uint16_t)data[0] | ((uint16_t)data[1] << 8U);
}

static uint16_t ConfigurationCrc(const uint8_t *data, uint16_t length)
{
  uint16_t crc = 0xFFFFU;

  for (uint16_t index = 0U; index < length; index++)
  {
    crc ^= (uint16_t)data[index] << 8U;
    for (uint8_t bit = 0U; bit < 8U; bit++)
    {
      crc = ((crc & 0x8000U) != 0U) ?
            (uint16_t)((crc << 1U) ^ 0x1021U) : (uint16_t)(crc << 1U);
    }
  }
  return crc;
}

static void SetDefaultConfiguration(void)
{
  configuration.shock_threshold_mg = 1600U;
  configuration.motion_delta_mg = 200U;
  configuration.motion_confirm_ms = 60U;
  configuration.still_confirm_ms = 1000U;
  configuration.shock_cooldown_ms = 500U;
  configuration.temperature_log_interval_s = 60U;
}

static uint8_t ConfigurationIsValid(const AppConfiguration *candidate)
{
  return ((candidate->shock_threshold_mg >= CONFIG_SHOCK_MIN_MG) &&
          (candidate->shock_threshold_mg <= CONFIG_SHOCK_MAX_MG) &&
          (candidate->motion_delta_mg >= CONFIG_MOTION_MIN_MG) &&
          (candidate->motion_delta_mg <= CONFIG_MOTION_MAX_MG) &&
          (candidate->motion_confirm_ms >= CONFIG_MOTION_CONFIRM_MIN_MS) &&
          (candidate->motion_confirm_ms <= CONFIG_MOTION_CONFIRM_MAX_MS) &&
          (candidate->still_confirm_ms >= CONFIG_STILL_CONFIRM_MIN_MS) &&
          (candidate->still_confirm_ms <= CONFIG_STILL_CONFIRM_MAX_MS) &&
          (candidate->shock_cooldown_ms >= CONFIG_COOLDOWN_MIN_MS) &&
          (candidate->shock_cooldown_ms <= CONFIG_COOLDOWN_MAX_MS) &&
          (candidate->temperature_log_interval_s >= CONFIG_TEMP_INTERVAL_MIN_S) &&
          (candidate->temperature_log_interval_s <= CONFIG_TEMP_INTERVAL_MAX_S)) ?
         1U : 0U;
}

static void EncodeConfiguration(uint8_t data[CONFIG_DATA_SIZE])
{
  (void)memset(data, 0U, CONFIG_DATA_SIZE);
  data[0] = 'T';
  data[1] = 'C';
  data[2] = 'F';
  data[3] = '1';
  PutUint16(&data[4], configuration.shock_threshold_mg);
  PutUint16(&data[6], configuration.motion_delta_mg);
  PutUint16(&data[8], configuration.motion_confirm_ms);
  PutUint16(&data[10], configuration.still_confirm_ms);
  PutUint16(&data[12], configuration.shock_cooldown_ms);
  PutUint16(&data[14], configuration.temperature_log_interval_s);
  PutUint16(&data[30], ConfigurationCrc(data, 30U));
}

static uint8_t DecodeConfiguration(const uint8_t data[CONFIG_DATA_SIZE])
{
  AppConfiguration candidate;

  if ((data[0] != 'T') || (data[1] != 'C') ||
      (data[2] != 'F') || (data[3] != '1') ||
      (GetUint16(&data[30]) != ConfigurationCrc(data, 30U)))
  {
    return 0U;
  }
  candidate.shock_threshold_mg = GetUint16(&data[4]);
  candidate.motion_delta_mg = GetUint16(&data[6]);
  candidate.motion_confirm_ms = GetUint16(&data[8]);
  candidate.still_confirm_ms = GetUint16(&data[10]);
  candidate.shock_cooldown_ms = GetUint16(&data[12]);
  candidate.temperature_log_interval_s = GetUint16(&data[14]);
  if (ConfigurationIsValid(&candidate) == 0U)
  {
    return 0U;
  }
  configuration = candidate;
  return 1U;
}

static uint8_t LoadConfiguration(void)
{
  uint8_t data[CONFIG_DATA_SIZE];

  if (W25Q64_Read(CONFIG_ADDRESS, data, sizeof(data)) != HAL_OK)
  {
    return 0U;
  }
  return DecodeConfiguration(data);
}

static HAL_StatusTypeDef SaveConfiguration(void)
{
  uint8_t data[CONFIG_DATA_SIZE];
  uint8_t readback[CONFIG_DATA_SIZE];

  if (flash_configuration_ready == 0U)
  {
    return HAL_ERROR;
  }
  EncodeConfiguration(data);
  if ((W25Q64_EraseSector(CONFIG_ADDRESS) != HAL_OK) ||
      (W25Q64_ProgramPage(CONFIG_ADDRESS, data, sizeof(data)) != HAL_OK) ||
      (W25Q64_Read(CONFIG_ADDRESS, readback, sizeof(readback)) != HAL_OK) ||
      (memcmp(data, readback, sizeof(data)) != 0))
  {
    return HAL_ERROR;
  }
  return HAL_OK;
}

static void SendConfiguration(void)
{
  SendStatus("@CFG,");
  SendUnsignedInteger(configuration.shock_threshold_mg);
  SendStatus(",");
  SendUnsignedInteger(configuration.motion_delta_mg);
  SendStatus(",");
  SendUnsignedInteger(configuration.motion_confirm_ms);
  SendStatus(",");
  SendUnsignedInteger(configuration.still_confirm_ms);
  SendStatus(",");
  SendUnsignedInteger(configuration.shock_cooldown_ms);
  SendStatus(",");
  SendUnsignedInteger(configuration.temperature_log_interval_s);
  SendStatus("\r\n");
}

static uint8_t ParseUnsigned(const char *text, uint16_t *value)
{
  uint32_t parsed = 0U;

  if ((text == NULL) || (*text == '\0'))
  {
    return 0U;
  }
  while (*text != '\0')
  {
    if ((*text < '0') || (*text > '9'))
    {
      return 0U;
    }
    parsed = (parsed * 10U) + (uint32_t)(*text - '0');
    if (parsed > 65535U)
    {
      return 0U;
    }
    text++;
  }
  *value = (uint16_t)parsed;
  return 1U;
}

static void ResetEventDetection(void)
{
  motion_active = 0U;
  motion_candidate_samples = 0U;
  still_samples = 0U;
}

static void ConfigurationChanged(void)
{
  ResetEventDetection();
  temperature_log_next_at = HAL_GetTick() +
    ((uint32_t)configuration.temperature_log_interval_s * 1000U);
  oled_refresh_pending = 1U;
}

static void HandleConfigurationCommand(const char *command)
{
  AppConfiguration candidate = configuration;
  const char *value_text;
  uint16_t value;

  if (strcmp(command, "cfg get") == 0)
  {
    SendConfiguration();
    return;
  }
  if (strcmp(command, "cfg defaults") == 0)
  {
    SetDefaultConfiguration();
    ConfigurationChanged();
    SendStatus("@CFG,DEFAULTS\r\n");
    SendConfiguration();
    return;
  }
  if (strcmp(command, "cfg save") == 0)
  {
    if (SaveConfiguration() == HAL_OK)
    {
      SendStatus("@CFG,SAVED\r\n");
    }
    else
    {
      SendStatus("@CFG,ERROR,FLASH\r\n");
    }
    return;
  }

  value_text = strrchr(command, ' ');
  if ((value_text == NULL) || (ParseUnsigned(value_text + 1, &value) == 0U))
  {
    SendStatus("@CFG,ERROR,FORMAT\r\n");
    return;
  }
  if (strncmp(command, "cfg shock ", 10U) == 0)
  {
    candidate.shock_threshold_mg = value;
  }
  else if (strncmp(command, "cfg motion ", 11U) == 0)
  {
    candidate.motion_delta_mg = value;
  }
  else if (strncmp(command, "cfg mconf ", 10U) == 0)
  {
    candidate.motion_confirm_ms = value;
  }
  else if (strncmp(command, "cfg sconf ", 10U) == 0)
  {
    candidate.still_confirm_ms = value;
  }
  else if (strncmp(command, "cfg cool ", 9U) == 0)
  {
    candidate.shock_cooldown_ms = value;
  }
  else if (strncmp(command, "cfg temp ", 9U) == 0)
  {
    candidate.temperature_log_interval_s = value;
  }
  else
  {
    SendStatus("@CFG,ERROR,FORMAT\r\n");
    return;
  }

  if (ConfigurationIsValid(&candidate) == 0U)
  {
    SendStatus("@CFG,ERROR,RANGE\r\n");
    return;
  }
  configuration = candidate;
  ConfigurationChanged();
  SendConfiguration();
}

static void SendBluetoothEvent(const char *event_name, uint16_t count)
{
  UART_HandleTypeDef *previous = SetOutputUart(ble_uart);

  SendStatus("@EVENT,");
  SendStatus(event_name);
  SendStatus(",");
  SendUnsignedInteger(count);
  SendStatus(",");
  if (rtc_ready != 0U)
  {
    SendDateTime(&latest_date_time);
  }
  else
  {
    SendStatus("NA");
  }
  SendStatus("\r\n");
  (void)SetOutputUart(previous);
}

static void SendBluetoothTemperature(int16_t temperature_tenths)
{
  UART_HandleTypeDef *previous = SetOutputUart(ble_uart);

  SendStatus("@TEMP,");
  SendSignedInteger(temperature_tenths);
  SendStatus(",");
  if (rtc_ready != 0U)
  {
    SendDateTime(&latest_date_time);
  }
  else
  {
    SendStatus("NA");
  }
  SendStatus("\r\n");
  (void)SetOutputUart(previous);
}

static void SendAcceleration(int16_t x_mg, int16_t y_mg, int16_t z_mg)
{
  SendStatus("ADXL345加速度(mg)：X=");
  SendSignedInteger(x_mg);
  SendStatus(" Y=");
  SendSignedInteger(y_mg);
  SendStatus(" Z=");
  SendSignedInteger(z_mg);
  SendStatus("\r\n");
}

static void AppendLogRecord(LogRecordType type, int16_t x_mg,
                            int16_t y_mg, int16_t z_mg)
{
  int16_t temperature = temperature_valid ? latest_temperature_tenths :
                                           (int16_t)0x8000;
  DataLogTimestamp timestamp;
  const DataLogTimestamp *timestamp_to_write = NULL;

  if ((flash_log_ready == 0U) || (logging_paused != 0U))
  {
    return;
  }
  if (rtc_ready != 0U)
  {
    timestamp.year_from_2000 = (uint8_t)(latest_date_time.year - 2000U);
    timestamp.month = latest_date_time.month;
    timestamp.day = latest_date_time.day;
    timestamp.hour = latest_date_time.hour;
    timestamp.minute = latest_date_time.minute;
    timestamp.second = latest_date_time.second;
    timestamp_to_write = &timestamp;
  }
  if (DataLogger_Append(type, HAL_GetTick() / 1000U, temperature,
                        x_mg, y_mg, z_mg, timestamp_to_write) != HAL_OK)
  {
    flash_log_ready = 0U;
    SendStatus("日志存储：写入、擦除或读回校验失败，已停止继续写入\r\n");
  }
}

static uint32_t AccelerationSquared(int16_t x_mg, int16_t y_mg,
                                    int16_t z_mg)
{
  int32_t x = x_mg;
  int32_t y = y_mg;
  int32_t z = z_mg;

  return (uint32_t)((x * x) + (y * y) + (z * z));
}

static uint32_t ThresholdSquared(uint16_t threshold_mg)
{
  uint32_t threshold = threshold_mg;

  return threshold * threshold;
}

static uint16_t SamplesForMilliseconds(uint16_t milliseconds)
{
  return (uint16_t)((milliseconds + ACCEL_SAMPLE_INTERVAL_MS - 1U) /
                    ACCEL_SAMPLE_INTERVAL_MS);
}

static void SetOledAlert(SSD1306_Alert alert, uint32_t now)
{
  if ((alert == SSD1306_ALERT_SHOCK) ||
      (oled_alert != SSD1306_ALERT_SHOCK) ||
      ((int32_t)(now - oled_alert_until) >= 0))
  {
    oled_alert = alert;
    oled_alert_until = now + OLED_ALERT_DURATION_MS;
    oled_refresh_pending = 1U;
  }
}

static void HandleAccelerationEvent(int16_t x_mg, int16_t y_mg,
                                    int16_t z_mg, uint32_t now)
{
  uint32_t magnitude_squared = AccelerationSquared(x_mg, y_mg, z_mg);
  uint16_t still_delta_mg = (configuration.motion_delta_mg > 50U) ?
                            (uint16_t)(configuration.motion_delta_mg - 50U) :
                            25U;
  uint16_t motion_low_mg = (uint16_t)(1000U - configuration.motion_delta_mg);
  uint16_t motion_high_mg = (uint16_t)(1000U + configuration.motion_delta_mg);
  uint16_t still_low_mg = (uint16_t)(1000U - still_delta_mg);
  uint16_t still_high_mg = (uint16_t)(1000U + still_delta_mg);

  if ((magnitude_squared >= ThresholdSquared(configuration.shock_threshold_mg)) &&
      ((shock_event_count == 0U) ||
       (now - shock_reported_at >= configuration.shock_cooldown_ms)))
  {
    shock_event_count++;
    shock_reported_at = now;
    SendStatus("事件：碰撞 #");
    SendUnsignedInteger(shock_event_count);
    SendStatus("\r\n");
    SendAcceleration(x_mg, y_mg, z_mg);
    AppendLogRecord(LOG_RECORD_SHOCK, x_mg, y_mg, z_mg);
    SetOledAlert(SSD1306_ALERT_SHOCK, now);
    SendBluetoothEvent("SHOCK", shock_event_count);
  }

  if (motion_active == 0U)
  {
    if ((magnitude_squared < ThresholdSquared(motion_low_mg)) ||
        (magnitude_squared > ThresholdSquared(motion_high_mg)))
    {
      if (++motion_candidate_samples >=
          SamplesForMilliseconds(configuration.motion_confirm_ms))
      {
        motion_active = 1U;
        motion_candidate_samples = 0U;
        still_samples = 0U;
        motion_event_count++;
        SendStatus("事件：运动开始 #");
        SendUnsignedInteger(motion_event_count);
        SendStatus("\r\n");
        AppendLogRecord(LOG_RECORD_MOTION_START, x_mg, y_mg, z_mg);
        SetOledAlert(SSD1306_ALERT_MOTION, now);
        SendBluetoothEvent("MOTION_START", motion_event_count);
      }
    }
    else
    {
      motion_candidate_samples = 0U;
    }
  }
  else if ((magnitude_squared >= ThresholdSquared(still_low_mg)) &&
           (magnitude_squared <= ThresholdSquared(still_high_mg)))
  {
    if (++still_samples >=
        SamplesForMilliseconds(configuration.still_confirm_ms))
    {
      motion_active = 0U;
      still_samples = 0U;
      SendStatus("事件：运动结束\r\n");
      AppendLogRecord(LOG_RECORD_MOTION_END, x_mg, y_mg, z_mg);
      SendBluetoothEvent("MOTION_END", motion_event_count);
    }
  }
  else
  {
    still_samples = 0U;
  }
}

static void FormatTemperature(int16_t temperature_tenths, char *text)
{
  uint8_t position = 0U;
  uint16_t magnitude;

  if (temperature_tenths < 0)
  {
    text[position++] = '-';
    magnitude = (uint16_t)(-temperature_tenths);
  }
  else
  {
    magnitude = (uint16_t)temperature_tenths;
  }

  if (magnitude >= 1000U)
  {
    text[position++] = (char)('0' + (magnitude / 1000U));
  }
  if (magnitude >= 100U)
  {
    text[position++] = (char)('0' + ((magnitude / 100U) % 10U));
  }
  text[position++] = (char)('0' + ((magnitude / 10U) % 10U));
  text[position++] = '.';
  text[position++] = (char)('0' + (magnitude % 10U));
  text[position++] = ' ';
  text[position++] = 'C';
  text[position] = '\0';
}

static void SendTemperature(const char *temperature_text)
{
  SendStatus("DS18B20温度：");
  SendStatus(temperature_text);
  SendStatus("\r\n");
}

static const char *LogTypeText(LogRecordType type)
{
  switch (type)
  {
    case LOG_RECORD_BOOT:
      return "BOOT";
    case LOG_RECORD_TEMPERATURE:
      return "TEMP";
    case LOG_RECORD_MOTION_START:
      return "MOTION_START";
    case LOG_RECORD_MOTION_END:
      return "MOTION_END";
    case LOG_RECORD_SHOCK:
      return "SHOCK";
    default:
      return "UNKNOWN";
  }
}

static void SendCsvTemperature(int16_t temperature_tenths)
{
  uint16_t magnitude;

  if (temperature_tenths == (int16_t)0x8000)
  {
    SendStatus("NA");
    return;
  }
  if (temperature_tenths < 0)
  {
    SendStatus("-");
    magnitude = (uint16_t)(-temperature_tenths);
  }
  else
  {
    magnitude = (uint16_t)temperature_tenths;
  }
  SendUnsignedInteger((uint16_t)(magnitude / 10U));
  SendStatus(".");
  SendUnsignedInteger((uint16_t)(magnitude % 10U));
}

static void SendStatusCommand(void)
{
  SendStatus("状态：温度=");
  SendStatus(temperature_valid ? latest_temperature_text : "无有效数据");
  SendStatus("，运动次数=");
  SendUnsignedInteger(motion_event_count);
  SendStatus("，碰撞次数=");
  SendUnsignedInteger(shock_event_count);
  SendStatus("，日志记录=");
  SendUnsignedInteger(DataLogger_GetRecordCount());
  SendStatus("，Flash读取=");
  SendStatus(flash_read_ready ? "正常" : "不可用");
  SendStatus("，Flash写入=");
  SendStatus(flash_log_ready ? "正常" : "故障");
  SendStatus("，记录状态=");
  SendStatus(logging_paused ? "已暂停" : "记录中");
  SendStatus("，时钟=");
  SendStatus(rtc_ready ? "正常" : "未设置或未连接");
  SendStatus("\r\n");
}

static void SendStatusJson(void)
{
  (void)UpdateRtcTime();
  SendStatus("@STATUS,");
  SendSignedInteger(temperature_valid ? latest_temperature_tenths :
                                      (int16_t)-32768);
  SendStatus(",");
  SendUnsignedInteger(motion_event_count);
  SendStatus(",");
  SendUnsignedInteger(shock_event_count);
  SendStatus(",");
  SendUnsignedInteger(DataLogger_GetRecordCount());
  SendStatus(",");
  SendUnsignedInteger(flash_read_ready);
  SendStatus(",");
  SendUnsignedInteger(flash_log_ready);
  SendStatus(",");
  SendUnsignedInteger(motion_active);
  SendStatus(",");
  SendSignedInteger(latest_x_mg);
  SendStatus(",");
  SendSignedInteger(latest_y_mg);
  SendStatus(",");
  SendSignedInteger(latest_z_mg);
  SendStatus(",");
  SendUnsignedInteger(logging_paused);
  SendStatus(",");
  if (rtc_ready != 0U)
  {
    SendDateTime(&latest_date_time);
  }
  else
  {
    SendStatus("NA");
  }
  SendStatus("\r\n");
}

static uint8_t CheckLogExportReady(void)
{
  if (flash_read_ready == 0U)
  {
    SendStatus("日志导出失败：Flash日志不可用\r\n");
    return 0U;
  }
  return 1U;
}

static void ExportLogsCsv(void)
{
  uint16_t record_count;
  uint16_t session = 0U;
  uint8_t session_has_data = 0U;

  if (CheckLogExportReady() == 0U)
  {
    return;
  }

  record_count = DataLogger_GetRecordCount();
  SendStatus("CSV_BEGIN\r\n");
  SendStatus("session,timestamp,time_s,temp_c,motion,shock\r\n");
  for (uint16_t index = 0U; index < record_count; index++)
  {
    DataLogRecord record;

    if (DataLogger_Read(index, &record) != HAL_OK)
    {
      SendStatus("CSV_ERROR,记录读取或CRC校验失败\r\n");
      break;
    }
    if (record.type == LOG_RECORD_BOOT)
    {
      session_has_data = 0U;
      continue;
    }

    if (session_has_data == 0U)
    {
      session++;
      session_has_data = 1U;
    }

    SendUnsignedInteger(session);
    SendStatus(",");
    SendLogTimestamp(&record.timestamp);
    SendStatus(",");
    SendUnsignedInteger32(record.uptime_seconds);
    SendStatus(",");
    SendCsvTemperature(record.temperature_tenths);
    SendStatus(",");
    if (record.type == LOG_RECORD_MOTION_START)
    {
      SendStatus("START");
    }
    else if (record.type == LOG_RECORD_MOTION_END)
    {
      SendStatus("END");
    }
    SendStatus(",");
    if (record.type == LOG_RECORD_SHOCK)
    {
      SendStatus("YES");
    }
    SendStatus("\r\n");
  }
  SendStatus("CSV_END\r\n");
}

static void ExportRawLogsCsv(void)
{
  uint16_t record_count;

  if (CheckLogExportReady() == 0U)
  {
    return;
  }

  record_count = DataLogger_GetRecordCount();
  SendStatus("CSV_RAW_BEGIN\r\n");
  SendStatus("sequence,type,timestamp,uptime_s,temp_c,x_mg,y_mg,z_mg\r\n");
  for (uint16_t index = 0U; index < record_count; index++)
  {
    DataLogRecord record;

    if (DataLogger_Read(index, &record) != HAL_OK)
    {
      SendStatus("CSV_ERROR,记录读取或CRC校验失败\r\n");
      break;
    }
    SendUnsignedInteger32(record.sequence);
    SendStatus(",");
    SendStatus(LogTypeText(record.type));
    SendStatus(",");
    SendLogTimestamp(&record.timestamp);
    SendStatus(",");
    SendUnsignedInteger32(record.uptime_seconds);
    SendStatus(",");
    SendCsvTemperature(record.temperature_tenths);
    SendStatus(",");
    SendSignedInteger(record.x_mg);
    SendStatus(",");
    SendSignedInteger(record.y_mg);
    SendStatus(",");
    SendSignedInteger(record.z_mg);
    SendStatus("\r\n");
  }
  SendStatus("CSV_RAW_END\r\n");
}

static uint8_t IsEventRecord(LogRecordType type)
{
  return ((type == LOG_RECORD_MOTION_START) ||
          (type == LOG_RECORD_MOTION_END) ||
          (type == LOG_RECORD_SHOCK)) ? 1U : 0U;
}

static void ExportRecentPreview(void)
{
  uint16_t record_count;
  uint16_t start_index;
  uint16_t session = 0U;

  if (CheckLogExportReady() == 0U)
  {
    return;
  }

  record_count = DataLogger_GetRecordCount();
  start_index = (record_count > 100U) ? (uint16_t)(record_count - 100U) : 0U;
  SendStatus("PREVIEW_BEGIN\r\n");
  SendStatus("sequence,session,timestamp,time_s,temp_c,event\r\n");
  for (uint16_t index = 0U; index < record_count; index++)
  {
    DataLogRecord record;

    if (DataLogger_Read(index, &record) != HAL_OK)
    {
      SendStatus("PREVIEW_ERROR,记录读取或CRC校验失败\r\n");
      break;
    }
    if (record.type == LOG_RECORD_BOOT)
    {
      session++;
      continue;
    }
    if (index < start_index)
    {
      continue;
    }
    if (session == 0U)
    {
      session = 1U;
    }

    SendUnsignedInteger32(record.sequence);
    SendStatus(",");
    SendUnsignedInteger(session);
    SendStatus(",");
    SendLogTimestamp(&record.timestamp);
    SendStatus(",");
    SendUnsignedInteger32(record.uptime_seconds);
    SendStatus(",");
    SendCsvTemperature(record.temperature_tenths);
    SendStatus(",");
    SendStatus(LogTypeText(record.type));
    SendStatus("\r\n");
  }
  SendStatus("PREVIEW_END\r\n");
}

static void ExportRecentEvents(void)
{
  uint16_t record_count;
  uint16_t total_events = 0U;
  uint16_t skipped_events;
  uint16_t session = 0U;

  if (CheckLogExportReady() == 0U)
  {
    return;
  }

  record_count = DataLogger_GetRecordCount();
  for (uint16_t index = 0U; index < record_count; index++)
  {
    DataLogRecord record;

    if (DataLogger_Read(index, &record) != HAL_OK)
    {
      SendStatus("EVENTS_ERROR,记录读取或CRC校验失败\r\n");
      return;
    }
    if (IsEventRecord(record.type) != 0U)
    {
      total_events++;
    }
  }

  skipped_events = (total_events > 200U) ?
                   (uint16_t)(total_events - 200U) : 0U;
  SendStatus("EVENTS_BEGIN,");
  SendUnsignedInteger(total_events);
  SendStatus("\r\n");
  SendStatus("sequence,session,timestamp,time_s,type,temp_c,x_mg,y_mg,z_mg\r\n");
  for (uint16_t index = 0U; index < record_count; index++)
  {
    DataLogRecord record;

    if (DataLogger_Read(index, &record) != HAL_OK)
    {
      SendStatus("EVENTS_ERROR,记录读取或CRC校验失败\r\n");
      break;
    }
    if (record.type == LOG_RECORD_BOOT)
    {
      session++;
      continue;
    }
    if (IsEventRecord(record.type) == 0U)
    {
      continue;
    }
    if (skipped_events != 0U)
    {
      skipped_events--;
      continue;
    }
    if (session == 0U)
    {
      session = 1U;
    }

    SendUnsignedInteger32(record.sequence);
    SendStatus(",");
    SendUnsignedInteger(session);
    SendStatus(",");
    SendLogTimestamp(&record.timestamp);
    SendStatus(",");
    SendUnsignedInteger32(record.uptime_seconds);
    SendStatus(",");
    SendStatus(LogTypeText(record.type));
    SendStatus(",");
    SendCsvTemperature(record.temperature_tenths);
    SendStatus(",");
    SendSignedInteger(record.x_mg);
    SendStatus(",");
    SendSignedInteger(record.y_mg);
    SendStatus(",");
    SendSignedInteger(record.z_mg);
    SendStatus("\r\n");
  }
  SendStatus("EVENTS_END\r\n");
}

static void ExecuteCommand(const char *command)
{
  if (strcmp(command, "help") == 0)
  {
    SendStatus("命令：help=帮助，status=当前状态，time=读取时钟，time set 2026-08-05 14:30:00=设置时钟，cfg get=读取参数，pause=暂停记录，resume=继续记录，sleep=休眠，preview=最近记录，events=运动碰撞，export=完整简表，export_raw=原始工程数据，clear=查看清空确认方法\r\n");
  }
  else if (strcmp(command, "status") == 0)
  {
    SendStatusCommand();
  }
  else if (strcmp(command, "status_json") == 0)
  {
    SendStatusJson();
  }
  else if ((strcmp(command, "time") == 0) ||
           (strncmp(command, "time set ", 9U) == 0))
  {
    HandleTimeCommand(command);
  }
  else if (strcmp(command, "preview") == 0)
  {
    ExportRecentPreview();
  }
  else if (strcmp(command, "events") == 0)
  {
    ExportRecentEvents();
  }
  else if (strncmp(command, "cfg ", 4U) == 0)
  {
    HandleConfigurationCommand(command);
  }
  else if (strcmp(command, "pause") == 0)
  {
    logging_paused = 1U;
    motion_active = 0U;
    motion_candidate_samples = 0U;
    still_samples = 0U;
    SendStatus("@CONTROL,PAUSED\r\n");
  }
  else if (strcmp(command, "resume") == 0)
  {
    logging_paused = 0U;
    temperature_log_next_at = 0U;
    SendStatus("@CONTROL,RUNNING\r\n");
  }
  else if (strcmp(command, "sleep") == 0)
  {
    SendStatus("@SLEEPING\r\n");
    HAL_Delay(100U);
    if (oled_ready != 0U)
    {
      (void)SSD1306_SetDisplayEnabled(0U);
    }
    HAL_GPIO_WritePin(STATUS_LED_GPIO_Port, STATUS_LED_Pin, GPIO_PIN_SET);
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_SuspendTick();
    HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);
    HAL_ResumeTick();
    (void)UpdateRtcTime();
    HAL_GPIO_WritePin(STATUS_LED_GPIO_Port, STATUS_LED_Pin, GPIO_PIN_RESET);
    led_updated_at = HAL_GetTick();
    if (oled_ready != 0U)
    {
      if (SSD1306_SetDisplayEnabled(1U) == HAL_OK)
      {
        oled_refresh_pending = 1U;
      }
      else
      {
        oled_ready = 0U;
      }
    }
    SendStatus("@AWAKE\r\n");
  }
  else if (strcmp(command, "wake") == 0)
  {
    SendStatusJson();
  }
  else if (strcmp(command, "export") == 0)
  {
    ExportLogsCsv();
  }
  else if (strcmp(command, "export_raw") == 0)
  {
    ExportRawLogsCsv();
  }
  else if (strcmp(command, "clear") == 0)
  {
    SendStatus("清空会删除全部温度和事件记录；确认请完整输入 clear confirm\r\n");
  }
  else if (strcmp(command, "clear confirm") == 0)
  {
    if (flash_read_ready == 0U)
    {
      SendStatus("日志清空失败：Flash日志不可用\r\n");
    }
    else
    {
      SendStatus("正在清空日志，请勿断电...\r\n");
      if (DataLogger_Clear() == HAL_OK)
      {
        flash_read_ready = 1U;
        flash_log_ready = 1U;
        motion_event_count = 0U;
        shock_event_count = 0U;
        AppendLogRecord(LOG_RECORD_BOOT, 0, 0, 0);
        oled_refresh_pending = 1U;
        SendStatus("日志已清空，并已建立新的启动会话\r\n");
      }
      else
      {
        flash_read_ready = 0U;
        flash_log_ready = 0U;
        SendStatus("日志清空失败：擦除过程中发生错误，请重新上电检查\r\n");
      }
    }
  }
  else
  {
    SendStatus("未知命令，请输入 help\r\n");
  }
}

static void ProcessUartInput(AppUartChannel *channel)
{
  if (channel->rx_overflow != 0U)
  {
    channel->rx_overflow = 0U;
    SendStatus("串口接收缓冲区已满，请重新输入\r\n");
  }

  while (channel->rx_tail != channel->rx_head)
  {
    UART_HandleTypeDef *previous = SetOutputUart(channel->uart);
    uint8_t character = channel->rx_buffer[channel->rx_tail];
    channel->rx_tail =
      (uint8_t)((channel->rx_tail + 1U) % UART_RX_BUFFER_SIZE);

    if ((character == '\r') || (character == '\n'))
    {
      if (channel->command_length != 0U)
      {
        channel->command_buffer[channel->command_length] = '\0';
        ExecuteCommand(channel->command_buffer);
        channel->command_length = 0U;
        if (channel == &console_channel)
        {
          SendStatus("> ");
        }
      }
    }
    else if ((character == 0x08U) || (character == 0x7FU))
    {
      if (channel->command_length != 0U)
      {
        channel->command_length--;
        if (channel == &console_channel)
        {
          SendStatus("\b \b");
        }
      }
    }
    else if ((character >= 0x20U) && (character <= 0x7EU))
    {
      if (channel->command_length < (COMMAND_BUFFER_SIZE - 1U))
      {
        if ((character >= 'A') && (character <= 'Z'))
        {
          character = (uint8_t)(character + ('a' - 'A'));
        }
        channel->command_buffer[channel->command_length++] = (char)character;
        if (channel == &console_channel)
        {
          HAL_UART_Transmit(channel->uart, &character, 1U, HAL_MAX_DELAY);
        }
      }
    }
    (void)SetOutputUart(previous);
  }
}

static uint8_t ScanI2cBus(I2C_HandleTypeDef *i2c, const char *bus_name,
                          uint8_t expected_address_1,
                          uint8_t expected_address_2)
{
  uint8_t found = 0U;
  uint8_t candidate = 0U;

  SendStatus(bus_name);
  SendStatus(" 开始扫描\r\n");
  for (uint8_t address = 1U; address < 127U; address++)
  {
    if (HAL_I2C_IsDeviceReady(i2c, (uint16_t)(address << 1U),
                              2U, 10U) == HAL_OK)
    {
      SendStatus("检测到I2C设备：0x");
      SendHexByte(address);
      SendStatus("\r\n");
      found++;
      if ((address == expected_address_1) ||
          (address == expected_address_2))
      {
        candidate = address;
      }
    }
  }

  if (found == 0U)
  {
    SendStatus("未检测到I2C设备\r\n");
  }
  SendStatus(bus_name);
  SendStatus(" 扫描完成\r\n");
  return candidate;
}

static HAL_StatusTypeDef RunFlashPersistenceTest(void)
{
  static const uint8_t marker[] = "LOGGER_FLASH_TEST_V1";
  uint8_t readback[sizeof(marker)];

  if (W25Q64_Read(FLASH_TEST_ADDRESS, readback, sizeof(readback)) != HAL_OK)
  {
    SendStatus("W25Q64测试：首次读取失败\r\n");
    return HAL_ERROR;
  }
  if (memcmp(readback, marker, sizeof(marker)) == 0)
  {
    SendStatus("W25Q64测试：复位或断电后标记仍然存在\r\n");
    return HAL_OK;
  }

  SendStatus("W25Q64测试：正在准备保留的最后一个扇区\r\n");
  if ((W25Q64_EraseSector(FLASH_TEST_ADDRESS) != HAL_OK) ||
      (W25Q64_ProgramPage(FLASH_TEST_ADDRESS, marker, sizeof(marker)) != HAL_OK) ||
      (W25Q64_Read(FLASH_TEST_ADDRESS, readback, sizeof(readback)) != HAL_OK) ||
      (memcmp(readback, marker, sizeof(marker)) != 0))
  {
    SendStatus("W25Q64测试：擦除、写入或读回校验失败\r\n");
    return HAL_ERROR;
  }

  SendStatus("W25Q64测试：标记写入并校验成功\r\n");
  SendStatus("W25Q64测试：请断电重启以验证掉电保存\r\n");
  return HAL_OK;
}

void App_Init(UART_HandleTypeDef *console_uart,
              UART_HandleTypeDef *bluetooth_uart,
              I2C_HandleTypeDef *display_bus,
              I2C_HandleTypeDef *motion_bus,
              SPI_HandleTypeDef *storage_bus)
{
  debug_uart = console_uart;
  ble_uart = bluetooth_uart;
  active_output_uart = debug_uart;
  (void)memset(&console_channel, 0, sizeof(console_channel));
  (void)memset(&bluetooth_channel, 0, sizeof(bluetooth_channel));
  console_channel.uart = debug_uart;
  bluetooth_channel.uart = ble_uart;
  display_i2c = display_bus;
  motion_i2c = motion_bus;
  conversion_pending = 0U;
  oled_ready = 0U;
  adxl_ready = 0U;
  flash_log_ready = 0U;
  flash_read_ready = 0U;
  flash_configuration_ready = 0U;
  rtc_ready = 0U;
  temperature_valid = 0U;
  adxl_i2c_address = 0U;
  adxl_read_failures = 0U;
  oled_refresh_pending = 1U;
  oled_figure_frame = 0U;
  motion_active = 0U;
  motion_candidate_samples = 0U;
  still_samples = 0U;
  motion_event_count = 0U;
  shock_event_count = 0U;
  latest_temperature_tenths = 0;
  latest_x_mg = 0;
  latest_y_mg = 0;
  latest_z_mg = 0;
  led_updated_at = HAL_GetTick();
  temperature_started_at = HAL_GetTick();
  temperature_next_at = HAL_GetTick();
  acceleration_sampled_at = HAL_GetTick();
  acceleration_reported_at = HAL_GetTick();
  shock_reported_at = HAL_GetTick();
  adxl_recovery_at = HAL_GetTick();
  temperature_log_next_at = 0U;
  oled_alert_until = 0U;
  oled_animated_at = HAL_GetTick();
  rtc_sampled_at = HAL_GetTick();
  oled_alert = SSD1306_ALERT_NONE;
  (void)memset(&latest_date_time, 0, sizeof(latest_date_time));
  SetDefaultConfiguration();
  (void)memcpy(latest_temperature_text, "--.- C", sizeof("--.- C"));

  DS18B20_Init(DS18B20_DQ_GPIO_Port, DS18B20_DQ_Pin);
  DS1302_Init(RTC_CLK_GPIO_Port, RTC_CLK_Pin,
              RTC_DAT_GPIO_Port, RTC_DAT_Pin,
              RTC_RST_GPIO_Port, RTC_RST_Pin);
  SendStatus("温度与运动记录器启动\r\n");
  SendStatus("DS18B20数据引脚=PA8，有线串口=115200，蓝牙串口=9600\r\n");
  SendStatus("DS1302引脚：CLK=PA4，DAT=PA5，RST=PA6\r\n");
  ReportRtcTime();
  uint8_t oled_address;
  uint8_t oled_initialized = 0U;
  uint8_t flash_id[3];

  HAL_Delay(OLED_POWER_ON_DELAY_MS);
  oled_address = ScanI2cBus(display_i2c, "I2C1 PB8/PB9", 0x3CU, 0x3DU);
  adxl_i2c_address =
    ScanI2cBus(motion_i2c, "I2C2 PB10/PB11", 0x53U, 0x1DU);
  if ((oled_address != 0U) &&
      (SSD1306_Init(display_i2c, oled_address) == HAL_OK))
  {
    oled_initialized = 1U;
  }

  if (oled_initialized == 0U)
  {
    SendStatus("SSD1306：冷启动尚未就绪，正在重试\r\n");
    HAL_Delay(OLED_INIT_RETRY_DELAY_MS);
    oled_address =
      ScanI2cBus(display_i2c, "I2C1 PB8/PB9 重试", 0x3CU, 0x3DU);
    if ((oled_address != 0U) &&
        (SSD1306_Init(display_i2c, oled_address) == HAL_OK))
    {
      oled_initialized = 1U;
    }
  }

  if (oled_initialized != 0U)
  {
    oled_ready = 1U;
    SendStatus("SSD1306：屏幕初始化成功\r\n");
  }
  else
  {
    SendStatus("SSD1306：两次初始化均失败\r\n");
  }

  if ((adxl_i2c_address != 0U) &&
      (ADXL345_Init(motion_i2c, adxl_i2c_address) == HAL_OK))
  {
    adxl_ready = 1U;
    SendStatus("ADXL345：设备ID为E5，测量准备完成\r\n");
    SendStatus("ADXL345：量程±8g，100Hz事件检测已启动\r\n");
  }
  else
  {
    SendStatus("ADXL345：未检测到设备或设备ID错误\r\n");
  }

  if (W25Q64_ReadJedecId(storage_bus, FLASH_CS_GPIO_Port, FLASH_CS_Pin,
                         flash_id) == HAL_OK)
  {
    SendStatus("SPI2 W25Q64芯片ID：");
    SendHexByte(flash_id[0]);
    SendStatus(" ");
    SendHexByte(flash_id[1]);
    SendStatus(" ");
    SendHexByte(flash_id[2]);
    SendStatus("\r\n");
    if (W25Q64_IsExpectedDevice(flash_id) != 0U)
    {
      SendStatus("W25Q64：检测到8MB存储器\r\n");
      flash_configuration_ready = 1U;
      if (LoadConfiguration() != 0U)
      {
        SendStatus("设备参数：已从W25Q64恢复保存值\r\n");
      }
      else
      {
        SetDefaultConfiguration();
        SendStatus("设备参数：未找到有效保存值，当前使用默认参数\r\n");
      }
      if (RunFlashPersistenceTest() == HAL_OK)
      {
        DataLoggerSummary log_summary;

        if (DataLogger_Init(&log_summary) == HAL_OK)
        {
          flash_read_ready = 1U;
          flash_log_ready = 1U;
          motion_event_count = log_summary.motion_events;
          shock_event_count = log_summary.shock_events;
          oled_refresh_pending = 1U;
          SendStatus("日志存储：发现已有记录 ");
          SendUnsignedInteger(log_summary.total_records);
          SendStatus(" 条\r\n");
          SendStatus("日志存储：恢复历史运动次数 ");
          SendUnsignedInteger(motion_event_count);
          SendStatus("，历史碰撞次数 ");
          SendUnsignedInteger(shock_event_count);
          SendStatus("\r\n");
          AppendLogRecord(LOG_RECORD_BOOT, 0, 0, 0);
          if (flash_log_ready != 0U)
          {
            SendStatus("日志存储：已追加本次启动记录，多扇区循环写入已启用\r\n");
          }
        }
        else
        {
          SendStatus("日志存储：扫描失败或发现损坏记录\r\n");
        }
      }
    }
    else
    {
      SendStatus("W25Q64：芯片ID与预期不一致\r\n");
    }
  }
  else
  {
    SendStatus("W25Q64：没有收到有效SPI响应\r\n");
  }

  if (HAL_UART_Receive_IT(console_channel.uart,
                          &console_channel.rx_byte, 1U) == HAL_OK)
  {
    SendStatus("串口命令已启用，输入 help 后按回车\r\n> ");
  }
  else
  {
    SendStatus("串口命令接收启动失败\r\n");
  }
  if (HAL_UART_Receive_IT(bluetooth_channel.uart,
                          &bluetooth_channel.rx_byte, 1U) != HAL_OK)
  {
    SendStatus("蓝牙命令接收启动失败\r\n");
  }
  else
  {
    UART_HandleTypeDef *previous;

    SendStatus("JDY-16命令通道已启用：USART2 PA2/PA3，9600波特率\r\n");
    previous = SetOutputUart(ble_uart);
    SendStatus("@AWAKE\r\n");
    (void)SetOutputUart(previous);
  }
}

void App_Process(void)
{
  if (HAL_GetTick() - led_updated_at >= 500U)
  {
    led_updated_at += 500U;
    HAL_GPIO_TogglePin(STATUS_LED_GPIO_Port, STATUS_LED_Pin);
  }

  ProcessUartInput(&console_channel);
  ProcessUartInput(&bluetooth_channel);

  if (HAL_GetTick() - rtc_sampled_at >= 1000U)
  {
    (void)UpdateRtcTime();
  }

  if ((conversion_pending == 0U) &&
      ((int32_t)(HAL_GetTick() - temperature_next_at) >= 0))
  {
    if (DS18B20_StartConversion() == HAL_OK)
    {
      conversion_pending = 1U;
      temperature_started_at = HAL_GetTick();
    }
    else
    {
      temperature_valid = 0U;
      (void)memcpy(latest_temperature_text, "--.- C", sizeof("--.- C"));
      oled_refresh_pending = 1U;
      SendStatus("DS18B20：未检测到温度传感器\r\n");
      temperature_next_at = HAL_GetTick() + 2000U;
    }
  }

  if ((conversion_pending != 0U) &&
      (HAL_GetTick() - temperature_started_at >= 750U))
  {
    int16_t raw_temperature;

    conversion_pending = 0U;
    temperature_next_at = HAL_GetTick() + 1250U;
    if (DS18B20_ReadTemperatureRaw(&raw_temperature) == HAL_OK)
    {
      int16_t temperature_tenths =
        (int16_t)(((int32_t)raw_temperature * 10) / 16);
      char temperature_text[9];

      FormatTemperature(temperature_tenths, temperature_text);
      (void)memcpy(latest_temperature_text, temperature_text,
                   sizeof(latest_temperature_text));
      oled_refresh_pending = 1U;
      latest_temperature_tenths = temperature_tenths;
      temperature_valid = 1U;
      SendTemperature(temperature_text);
      SendBluetoothTemperature(temperature_tenths);
      if ((temperature_log_next_at == 0U) ||
          ((int32_t)(HAL_GetTick() - temperature_log_next_at) >= 0))
      {
        AppendLogRecord(LOG_RECORD_TEMPERATURE, latest_x_mg,
                        latest_y_mg, latest_z_mg);
        temperature_log_next_at = HAL_GetTick() +
          ((uint32_t)configuration.temperature_log_interval_s * 1000U);
      }
    }
    else
    {
      temperature_valid = 0U;
      (void)memcpy(latest_temperature_text, "--.- C", sizeof("--.- C"));
      oled_refresh_pending = 1U;
      SendStatus("DS18B20：传感器断开或数据错误\r\n");
    }
  }

  if ((adxl_ready != 0U) &&
      (HAL_GetTick() - acceleration_sampled_at >= ACCEL_SAMPLE_INTERVAL_MS))
  {
    int16_t x_mg;
    int16_t y_mg;
    int16_t z_mg;
    uint32_t now = HAL_GetTick();

    acceleration_sampled_at = now;
    if (ADXL345_ReadMillig(&x_mg, &y_mg, &z_mg) == HAL_OK)
    {
      adxl_read_failures = 0U;
      latest_x_mg = x_mg;
      latest_y_mg = y_mg;
      latest_z_mg = z_mg;
      if (logging_paused == 0U)
      {
        HandleAccelerationEvent(x_mg, y_mg, z_mg, now);
      }
      if (now - acceleration_reported_at >= ACCEL_REPORT_INTERVAL_MS)
      {
        acceleration_reported_at = now;
        SendAcceleration(x_mg, y_mg, z_mg);
      }
    }
    else
    {
      if (++adxl_read_failures >= ADXL_FAILURE_LIMIT)
      {
        adxl_ready = 0U;
        adxl_recovery_at = now;
        SendStatus("ADXL345：连续读取失败，正在恢复通信\r\n");
      }
    }
  }

  if ((adxl_ready == 0U) && (adxl_i2c_address != 0U) &&
      (HAL_GetTick() - adxl_recovery_at >= ADXL_RECOVERY_INTERVAL_MS))
  {
    adxl_recovery_at = HAL_GetTick();
    HAL_I2C_DeInit(motion_i2c);
    HAL_Delay(1U);
    if ((HAL_I2C_Init(motion_i2c) == HAL_OK) &&
        (ADXL345_Init(motion_i2c, adxl_i2c_address) == HAL_OK))
    {
      adxl_ready = 1U;
      adxl_read_failures = 0U;
      acceleration_sampled_at = HAL_GetTick();
      SendStatus("ADXL345：通信已恢复\r\n");
    }
  }

  if ((oled_alert != SSD1306_ALERT_NONE) &&
      ((int32_t)(HAL_GetTick() - oled_alert_until) >= 0))
  {
    oled_alert = SSD1306_ALERT_NONE;
    oled_refresh_pending = 1U;
  }

  if ((oled_ready != 0U) && (oled_refresh_pending != 0U))
  {
    oled_refresh_pending = 0U;
    if (SSD1306_ShowLoggerStatus(latest_temperature_text,
                                 shock_event_count, motion_event_count,
                                 oled_alert, oled_figure_frame) != HAL_OK)
    {
      oled_ready = 0U;
      SendStatus("SSD1306：屏幕刷新失败\r\n");
    }
  }
  else if ((oled_ready != 0U) &&
           (HAL_GetTick() - oled_animated_at >=
            OLED_ANIMATION_INTERVAL_MS))
  {
    oled_animated_at = HAL_GetTick();
    oled_figure_frame ^= 1U;
    if (SSD1306_UpdateFigure(oled_alert, oled_figure_frame) != HAL_OK)
    {
      oled_ready = 0U;
      SendStatus("SSD1306：人物动画刷新失败\r\n");
    }
  }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *uart)
{
  AppUartChannel *channel = NULL;

  if (uart == console_channel.uart)
  {
    channel = &console_channel;
  }
  else if (uart == bluetooth_channel.uart)
  {
    channel = &bluetooth_channel;
  }

  if (channel != NULL)
  {
    uint8_t next_head =
      (uint8_t)((channel->rx_head + 1U) % UART_RX_BUFFER_SIZE);

    if (next_head != channel->rx_tail)
    {
      channel->rx_buffer[channel->rx_head] = channel->rx_byte;
      channel->rx_head = next_head;
    }
    else
    {
      channel->rx_overflow = 1U;
    }
    (void)HAL_UART_Receive_IT(channel->uart, &channel->rx_byte, 1U);
  }
}
