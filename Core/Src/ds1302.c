#include "ds1302.h"

#define DS1302_CLOCK_BURST_READ 0xBFU
#define DS1302_CLOCK_BURST_WRITE 0xBEU
#define DS1302_WRITE_PROTECT_WRITE 0x8EU
#define DS1302_TRICKLE_CHARGE_WRITE 0x90U

static GPIO_TypeDef *clock_port;
static GPIO_TypeDef *data_port;
static GPIO_TypeDef *reset_port;
static uint16_t clock_pin;
static uint16_t data_pin;
static uint16_t reset_pin;

static void DelayMicroseconds(uint32_t microseconds)
{
  uint32_t cycles = (SystemCoreClock / 1000000U) * microseconds;
  uint32_t started_at = DWT->CYCCNT;

  while ((DWT->CYCCNT - started_at) < cycles)
  {
  }
}

static void DataPinAsOutput(void)
{
  GPIO_InitTypeDef gpio = {0};

  gpio.Pin = data_pin;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(data_port, &gpio);
}

static void DataPinAsInput(void)
{
  GPIO_InitTypeDef gpio = {0};

  gpio.Pin = data_pin;
  gpio.Mode = GPIO_MODE_INPUT;
  gpio.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(data_port, &gpio);
}

static void BeginTransaction(void)
{
  HAL_GPIO_WritePin(reset_port, reset_pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(clock_port, clock_pin, GPIO_PIN_RESET);
  DataPinAsOutput();
  HAL_GPIO_WritePin(reset_port, reset_pin, GPIO_PIN_SET);
  DelayMicroseconds(4U);
}

static void EndTransaction(void)
{
  HAL_GPIO_WritePin(reset_port, reset_pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(clock_port, clock_pin, GPIO_PIN_RESET);
  DataPinAsOutput();
  HAL_GPIO_WritePin(data_port, data_pin, GPIO_PIN_RESET);
  DelayMicroseconds(4U);
}

static void WriteByteLsbFirst(uint8_t value)
{
  for (uint8_t bit = 0U; bit < 8U; bit++)
  {
    HAL_GPIO_WritePin(data_port, data_pin,
                      ((value & 0x01U) != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    DelayMicroseconds(1U);
    HAL_GPIO_WritePin(clock_port, clock_pin, GPIO_PIN_SET);
    DelayMicroseconds(1U);
    HAL_GPIO_WritePin(clock_port, clock_pin, GPIO_PIN_RESET);
    value >>= 1U;
  }
}

static void WriteReadCommand(uint8_t command)
{
  for (uint8_t bit = 0U; bit < 8U; bit++)
  {
    HAL_GPIO_WritePin(data_port, data_pin,
                      ((command & 0x01U) != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    DelayMicroseconds(1U);
    HAL_GPIO_WritePin(clock_port, clock_pin, GPIO_PIN_SET);
    DelayMicroseconds(1U);
    if (bit == 7U)
    {
      DataPinAsInput();
    }
    HAL_GPIO_WritePin(clock_port, clock_pin, GPIO_PIN_RESET);
    DelayMicroseconds(1U);
    command >>= 1U;
  }
}

static uint8_t ReadByteLsbFirst(void)
{
  uint8_t value = 0U;

  for (uint8_t bit = 0U; bit < 8U; bit++)
  {
    if (HAL_GPIO_ReadPin(data_port, data_pin) == GPIO_PIN_SET)
    {
      value |= (uint8_t)(1U << bit);
    }
    HAL_GPIO_WritePin(clock_port, clock_pin, GPIO_PIN_SET);
    DelayMicroseconds(1U);
    HAL_GPIO_WritePin(clock_port, clock_pin, GPIO_PIN_RESET);
    DelayMicroseconds(1U);
  }
  return value;
}

static void WriteRegister(uint8_t command, uint8_t value)
{
  BeginTransaction();
  WriteByteLsbFirst(command);
  WriteByteLsbFirst(value);
  EndTransaction();
}

static uint8_t BcdToBinary(uint8_t value)
{
  return (uint8_t)(((value >> 4U) * 10U) + (value & 0x0FU));
}

static uint8_t BinaryToBcd(uint8_t value)
{
  return (uint8_t)(((value / 10U) << 4U) | (value % 10U));
}

static uint8_t DaysInMonth(uint16_t year, uint8_t month)
{
  static const uint8_t days[] =
    {31U, 28U, 31U, 30U, 31U, 30U, 31U, 31U, 30U, 31U, 30U, 31U};
  uint8_t result;

  if ((month < 1U) || (month > 12U))
  {
    return 0U;
  }
  result = days[month - 1U];
  if ((month == 2U) && ((year % 4U) == 0U))
  {
    result = 29U;
  }
  return result;
}

void DS1302_Init(GPIO_TypeDef *clk_port, uint16_t clk_gpio_pin,
                 GPIO_TypeDef *dat_port, uint16_t dat_gpio_pin,
                 GPIO_TypeDef *rst_port, uint16_t rst_gpio_pin)
{
  clock_port = clk_port;
  clock_pin = clk_gpio_pin;
  data_port = dat_port;
  data_pin = dat_gpio_pin;
  reset_port = rst_port;
  reset_pin = rst_gpio_pin;

  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0U;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
  EndTransaction();
}

uint8_t DS1302_DateTimeIsValid(const DS1302_DateTime *date_time)
{
  if ((date_time == NULL) ||
      (date_time->year < 2000U) || (date_time->year > 2099U) ||
      (date_time->month < 1U) || (date_time->month > 12U) ||
      (date_time->day < 1U) ||
      (date_time->day > DaysInMonth(date_time->year, date_time->month)) ||
      (date_time->weekday < 1U) || (date_time->weekday > 7U) ||
      (date_time->hour > 23U) || (date_time->minute > 59U) ||
      (date_time->second > 59U))
  {
    return 0U;
  }
  return 1U;
}

HAL_StatusTypeDef DS1302_ReadDateTime(DS1302_DateTime *date_time)
{
  uint8_t raw[8];

  if (date_time == NULL)
  {
    return HAL_ERROR;
  }
  BeginTransaction();
  WriteReadCommand(DS1302_CLOCK_BURST_READ);
  for (uint8_t index = 0U; index < sizeof(raw); index++)
  {
    raw[index] = ReadByteLsbFirst();
  }
  EndTransaction();

  if ((raw[0] & 0x80U) != 0U)
  {
    return HAL_ERROR;
  }
  date_time->second = BcdToBinary((uint8_t)(raw[0] & 0x7FU));
  date_time->minute = BcdToBinary((uint8_t)(raw[1] & 0x7FU));
  date_time->hour = BcdToBinary((uint8_t)(raw[2] & 0x3FU));
  date_time->day = BcdToBinary((uint8_t)(raw[3] & 0x3FU));
  date_time->month = BcdToBinary((uint8_t)(raw[4] & 0x1FU));
  date_time->weekday = BcdToBinary((uint8_t)(raw[5] & 0x07U));
  date_time->year = (uint16_t)(2000U + BcdToBinary(raw[6]));

  return (DS1302_DateTimeIsValid(date_time) != 0U) ? HAL_OK : HAL_ERROR;
}

HAL_StatusTypeDef DS1302_WriteDateTime(const DS1302_DateTime *date_time)
{
  uint8_t raw[8];

  if (DS1302_DateTimeIsValid(date_time) == 0U)
  {
    return HAL_ERROR;
  }

  raw[0] = BinaryToBcd(date_time->second);
  raw[1] = BinaryToBcd(date_time->minute);
  raw[2] = BinaryToBcd(date_time->hour);
  raw[3] = BinaryToBcd(date_time->day);
  raw[4] = BinaryToBcd(date_time->month);
  raw[5] = BinaryToBcd(date_time->weekday);
  raw[6] = BinaryToBcd((uint8_t)(date_time->year - 2000U));
  raw[7] = 0U;

  WriteRegister(DS1302_WRITE_PROTECT_WRITE, 0U);
  WriteRegister(DS1302_TRICKLE_CHARGE_WRITE, 0U);
  BeginTransaction();
  WriteByteLsbFirst(DS1302_CLOCK_BURST_WRITE);
  for (uint8_t index = 0U; index < sizeof(raw); index++)
  {
    WriteByteLsbFirst(raw[index]);
  }
  EndTransaction();
  WriteRegister(DS1302_WRITE_PROTECT_WRITE, 0x80U);
  return HAL_OK;
}
