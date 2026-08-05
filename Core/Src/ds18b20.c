#include "ds18b20.h"

static GPIO_TypeDef *dq_port;
static uint16_t dq_pin;
static uint32_t cycles_per_us;

static void DelayUs(uint32_t microseconds)
{
  uint32_t start = DWT->CYCCNT;
  uint32_t cycles = microseconds * cycles_per_us;

  while ((DWT->CYCCNT - start) < cycles)
  {
  }
}

static void DqLow(void)
{
  HAL_GPIO_WritePin(dq_port, dq_pin, GPIO_PIN_RESET);
}

static void DqRelease(void)
{
  HAL_GPIO_WritePin(dq_port, dq_pin, GPIO_PIN_SET);
}

static HAL_StatusTypeDef ResetBus(void)
{
  GPIO_PinState presence;

  DqLow();
  DelayUs(480U);
  DqRelease();
  DelayUs(70U);
  presence = HAL_GPIO_ReadPin(dq_port, dq_pin);
  DelayUs(410U);

  return (presence == GPIO_PIN_RESET) ? HAL_OK : HAL_ERROR;
}

static void WriteBit(uint8_t bit)
{
  DqLow();
  if (bit != 0U)
  {
    DelayUs(6U);
    DqRelease();
    DelayUs(64U);
  }
  else
  {
    DelayUs(60U);
    DqRelease();
    DelayUs(10U);
  }
}

static uint8_t ReadBit(void)
{
  uint8_t bit;

  DqLow();
  DelayUs(3U);
  DqRelease();
  DelayUs(10U);
  bit = (HAL_GPIO_ReadPin(dq_port, dq_pin) == GPIO_PIN_SET) ? 1U : 0U;
  DelayUs(53U);

  return bit;
}

static void WriteByte(uint8_t value)
{
  for (uint8_t bit = 0U; bit < 8U; bit++)
  {
    WriteBit(value & 0x01U);
    value >>= 1U;
  }
}

static uint8_t ReadByte(void)
{
  uint8_t value = 0U;

  for (uint8_t bit = 0U; bit < 8U; bit++)
  {
    value |= (uint8_t)(ReadBit() << bit);
  }

  return value;
}

static uint8_t CalculateCrc(const uint8_t *data, uint8_t length)
{
  uint8_t crc = 0U;

  for (uint8_t index = 0U; index < length; index++)
  {
    uint8_t value = data[index];
    for (uint8_t bit = 0U; bit < 8U; bit++)
    {
      uint8_t mix = (uint8_t)((crc ^ value) & 0x01U);
      crc >>= 1U;
      if (mix != 0U)
      {
        crc ^= 0x8CU;
      }
      value >>= 1U;
    }
  }

  return crc;
}

void DS18B20_Init(GPIO_TypeDef *port, uint16_t pin)
{
  dq_port = port;
  dq_pin = pin;
  cycles_per_us = HAL_RCC_GetHCLKFreq() / 1000000U;

  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0U;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
  DqRelease();
}

HAL_StatusTypeDef DS18B20_StartConversion(void)
{
  if (ResetBus() != HAL_OK)
  {
    return HAL_ERROR;
  }

  WriteByte(0xCCU);
  WriteByte(0x44U);
  return HAL_OK;
}

HAL_StatusTypeDef DS18B20_ReadTemperatureRaw(int16_t *raw_temperature)
{
  uint8_t scratchpad[9];
  uint8_t all_zero = 1U;
  uint8_t all_one = 1U;
  int16_t temperature;

  if ((raw_temperature == NULL) || (ResetBus() != HAL_OK))
  {
    return HAL_ERROR;
  }

  WriteByte(0xCCU);
  WriteByte(0xBEU);
  for (uint8_t index = 0U; index < sizeof(scratchpad); index++)
  {
    scratchpad[index] = ReadByte();
    if (scratchpad[index] != 0x00U)
    {
      all_zero = 0U;
    }
    if (scratchpad[index] != 0xFFU)
    {
      all_one = 0U;
    }
  }

  if ((all_zero != 0U) || (all_one != 0U) ||
      (CalculateCrc(scratchpad, 8U) != scratchpad[8]))
  {
    return HAL_ERROR;
  }

  temperature = (int16_t)(((uint16_t)scratchpad[1] << 8U) | scratchpad[0]);
  if ((temperature < (-55 * 16)) || (temperature > (125 * 16)))
  {
    return HAL_ERROR;
  }

  *raw_temperature = temperature;
  return HAL_OK;
}
