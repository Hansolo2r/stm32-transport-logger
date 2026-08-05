#include "w25q64.h"

#define W25Q64_CMD_READ_JEDEC_ID 0x9FU
#define W25Q64_CMD_RELEASE_POWER_DOWN 0xABU
#define W25Q64_CMD_READ_DATA     0x03U
#define W25Q64_CMD_WRITE_ENABLE  0x06U
#define W25Q64_CMD_READ_STATUS_1 0x05U
#define W25Q64_CMD_SECTOR_ERASE  0x20U
#define W25Q64_CMD_PAGE_PROGRAM  0x02U
#define W25Q64_STATUS_BUSY       0x01U
#define W25Q64_CAPACITY_BYTES    0x00800000UL

static SPI_HandleTypeDef *flash_spi;
static GPIO_TypeDef *flash_cs_port;
static uint16_t flash_cs_pin;

static void Select(void)
{
  HAL_GPIO_WritePin(flash_cs_port, flash_cs_pin, GPIO_PIN_RESET);
}

static void Deselect(void)
{
  HAL_GPIO_WritePin(flash_cs_port, flash_cs_pin, GPIO_PIN_SET);
}

static HAL_StatusTypeDef WriteEnable(void)
{
  uint8_t command = W25Q64_CMD_WRITE_ENABLE;
  HAL_StatusTypeDef status;

  Select();
  status = HAL_SPI_Transmit(flash_spi, &command, 1U, 100U);
  Deselect();
  return status;
}

static HAL_StatusTypeDef ReleasePowerDown(void)
{
  uint8_t command = W25Q64_CMD_RELEASE_POWER_DOWN;
  HAL_StatusTypeDef status;

  Deselect();
  HAL_Delay(1U);
  Select();
  status = HAL_SPI_Transmit(flash_spi, &command, 1U, 100U);
  Deselect();
  HAL_Delay(1U);
  return status;
}

static HAL_StatusTypeDef WaitUntilReady(uint32_t timeout_ms)
{
  uint32_t started_at = HAL_GetTick();
  uint8_t transmit_data[2] = {W25Q64_CMD_READ_STATUS_1, 0xFFU};
  uint8_t receive_data[2];

  do
  {
    HAL_StatusTypeDef status;

    Select();
    status = HAL_SPI_TransmitReceive(flash_spi, transmit_data, receive_data,
                                    sizeof(transmit_data), 100U);
    Deselect();
    if (status != HAL_OK)
    {
      return status;
    }
    if ((receive_data[1] & W25Q64_STATUS_BUSY) == 0U)
    {
      return HAL_OK;
    }
  } while (HAL_GetTick() - started_at < timeout_ms);

  return HAL_TIMEOUT;
}

HAL_StatusTypeDef W25Q64_ReadJedecId(SPI_HandleTypeDef *spi,
                                    GPIO_TypeDef *cs_port, uint16_t cs_pin,
                                    uint8_t jedec_id[3])
{
  uint8_t transmit_data[4] = {W25Q64_CMD_READ_JEDEC_ID, 0xFFU, 0xFFU, 0xFFU};
  uint8_t receive_data[4] = {0U};
  HAL_StatusTypeDef status;

  if ((spi == NULL) || (cs_port == NULL) || (jedec_id == NULL))
  {
    return HAL_ERROR;
  }

  flash_spi = spi;
  flash_cs_port = cs_port;
  flash_cs_pin = cs_pin;
  if (ReleasePowerDown() != HAL_OK)
  {
    return HAL_ERROR;
  }

  HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_RESET);
  status = HAL_SPI_TransmitReceive(spi, transmit_data, receive_data,
                                  sizeof(transmit_data), 100U);
  HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_SET);

  if (status != HAL_OK)
  {
    return status;
  }

  jedec_id[0] = receive_data[1];
  jedec_id[1] = receive_data[2];
  jedec_id[2] = receive_data[3];
  return HAL_OK;
}

uint8_t W25Q64_IsExpectedDevice(const uint8_t jedec_id[3])
{
  if (jedec_id == NULL)
  {
    return 0U;
  }

  return ((jedec_id[0] == 0xEFU) && (jedec_id[1] == 0x40U) &&
          (jedec_id[2] == 0x17U)) ? 1U : 0U;
}

HAL_StatusTypeDef W25Q64_Read(uint32_t address, uint8_t *data,
                             uint16_t length)
{
  uint8_t command[4];
  HAL_StatusTypeDef status;

  if ((flash_spi == NULL) || (data == NULL) || (length == 0U) ||
      (address + length > W25Q64_CAPACITY_BYTES))
  {
    return HAL_ERROR;
  }

  command[0] = W25Q64_CMD_READ_DATA;
  command[1] = (uint8_t)(address >> 16U);
  command[2] = (uint8_t)(address >> 8U);
  command[3] = (uint8_t)address;
  Select();
  status = HAL_SPI_Transmit(flash_spi, command, sizeof(command), 100U);
  if (status == HAL_OK)
  {
    status = HAL_SPI_Receive(flash_spi, data, length, 100U);
  }
  Deselect();
  return status;
}

HAL_StatusTypeDef W25Q64_EraseSector(uint32_t address)
{
  uint8_t command[4];
  HAL_StatusTypeDef status;

  if ((flash_spi == NULL) || (address >= W25Q64_CAPACITY_BYTES))
  {
    return HAL_ERROR;
  }

  address &= 0x00FFF000UL;
  if (WriteEnable() != HAL_OK)
  {
    return HAL_ERROR;
  }

  command[0] = W25Q64_CMD_SECTOR_ERASE;
  command[1] = (uint8_t)(address >> 16U);
  command[2] = (uint8_t)(address >> 8U);
  command[3] = (uint8_t)address;
  Select();
  status = HAL_SPI_Transmit(flash_spi, command, sizeof(command), 100U);
  Deselect();
  if (status != HAL_OK)
  {
    return status;
  }
  return WaitUntilReady(1000U);
}

HAL_StatusTypeDef W25Q64_ProgramPage(uint32_t address, const uint8_t *data,
                                    uint16_t length)
{
  uint8_t command[4];
  HAL_StatusTypeDef status;

  if ((flash_spi == NULL) || (data == NULL) || (length == 0U) ||
      (length > 256U) || (((address & 0xFFU) + length) > 256U) ||
      (address + length > W25Q64_CAPACITY_BYTES))
  {
    return HAL_ERROR;
  }
  if (WriteEnable() != HAL_OK)
  {
    return HAL_ERROR;
  }

  command[0] = W25Q64_CMD_PAGE_PROGRAM;
  command[1] = (uint8_t)(address >> 16U);
  command[2] = (uint8_t)(address >> 8U);
  command[3] = (uint8_t)address;
  Select();
  status = HAL_SPI_Transmit(flash_spi, command, sizeof(command), 100U);
  if (status == HAL_OK)
  {
    status = HAL_SPI_Transmit(flash_spi, data, length, 100U);
  }
  Deselect();
  if (status != HAL_OK)
  {
    return status;
  }
  return WaitUntilReady(100U);
}
