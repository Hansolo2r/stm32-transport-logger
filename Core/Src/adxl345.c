#include "adxl345.h"

#define ADXL345_REG_DEVID       0x00U
#define ADXL345_REG_BW_RATE     0x2CU
#define ADXL345_REG_POWER_CTL   0x2DU
#define ADXL345_REG_DATA_FORMAT 0x31U
#define ADXL345_REG_DATAX0      0x32U
#define ADXL345_DEVICE_ID       0xE5U

static I2C_HandleTypeDef *adxl_i2c;
static uint16_t adxl_address;

static HAL_StatusTypeDef WriteRegister(uint8_t reg, uint8_t value)
{
  return HAL_I2C_Mem_Write(adxl_i2c, adxl_address, reg,
                           I2C_MEMADD_SIZE_8BIT, &value, 1U, 100U);
}

static HAL_StatusTypeDef ReadRegisters(uint8_t reg, uint8_t *data,
                                       uint16_t length)
{
  return HAL_I2C_Mem_Read(adxl_i2c, adxl_address, reg,
                          I2C_MEMADD_SIZE_8BIT, data, length, 100U);
}

HAL_StatusTypeDef ADXL345_Init(I2C_HandleTypeDef *i2c, uint8_t address)
{
  uint8_t device_id;

  adxl_i2c = i2c;
  adxl_address = (uint16_t)(address << 1U);

  if ((ReadRegisters(ADXL345_REG_DEVID, &device_id, 1U) != HAL_OK) ||
      (device_id != ADXL345_DEVICE_ID))
  {
    return HAL_ERROR;
  }

  /* Full-resolution mode with a +/-8 g measurement range. */
  if (WriteRegister(ADXL345_REG_DATA_FORMAT, 0x0AU) != HAL_OK)
  {
    return HAL_ERROR;
  }
  if (WriteRegister(ADXL345_REG_BW_RATE, 0x0AU) != HAL_OK)
  {
    return HAL_ERROR;
  }
  if (WriteRegister(ADXL345_REG_POWER_CTL, 0x08U) != HAL_OK)
  {
    return HAL_ERROR;
  }

  return HAL_OK;
}

HAL_StatusTypeDef ADXL345_ReadMillig(int16_t *x_mg, int16_t *y_mg,
                                    int16_t *z_mg)
{
  uint8_t data[6];
  int16_t x_raw;
  int16_t y_raw;
  int16_t z_raw;

  if ((x_mg == NULL) || (y_mg == NULL) || (z_mg == NULL) ||
      (ReadRegisters(ADXL345_REG_DATAX0, data, sizeof(data)) != HAL_OK))
  {
    return HAL_ERROR;
  }

  x_raw = (int16_t)(((uint16_t)data[1] << 8U) | data[0]);
  y_raw = (int16_t)(((uint16_t)data[3] << 8U) | data[2]);
  z_raw = (int16_t)(((uint16_t)data[5] << 8U) | data[4]);

  *x_mg = (int16_t)(x_raw * 4);
  *y_mg = (int16_t)(y_raw * 4);
  *z_mg = (int16_t)(z_raw * 4);
  return HAL_OK;
}
