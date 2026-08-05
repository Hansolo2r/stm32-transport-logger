#include "ssd1306.h"

#include <string.h>

#define OLED_WIDTH 128U
#define OLED_HEIGHT 32U
#define OLED_PAGES (OLED_HEIGHT / 8U)
#define FIGURE_PANEL_LEFT 80U

typedef struct
{
  char character;
  uint8_t columns[5];
} Glyph_t;

static const Glyph_t font[] = {
  {' ', {0x00, 0x00, 0x00, 0x00, 0x00}},
  {'!', {0x00, 0x00, 0x5F, 0x00, 0x00}},
  {'+', {0x08, 0x08, 0x3E, 0x08, 0x08}},
  {'-', {0x08, 0x08, 0x08, 0x08, 0x08}},
  {'.', {0x00, 0x60, 0x60, 0x00, 0x00}},
  {'0', {0x3E, 0x51, 0x49, 0x45, 0x3E}},
  {'1', {0x00, 0x42, 0x7F, 0x40, 0x00}},
  {'2', {0x42, 0x61, 0x51, 0x49, 0x46}},
  {'3', {0x21, 0x41, 0x45, 0x4B, 0x31}},
  {'4', {0x18, 0x14, 0x12, 0x7F, 0x10}},
  {'5', {0x27, 0x45, 0x45, 0x45, 0x39}},
  {'6', {0x3C, 0x4A, 0x49, 0x49, 0x30}},
  {'7', {0x01, 0x71, 0x09, 0x05, 0x03}},
  {'8', {0x36, 0x49, 0x49, 0x49, 0x36}},
  {'9', {0x06, 0x49, 0x49, 0x29, 0x1E}},
  {'A', {0x7E, 0x11, 0x11, 0x11, 0x7E}},
  {'C', {0x3E, 0x41, 0x41, 0x41, 0x22}},
  {'D', {0x7F, 0x41, 0x41, 0x22, 0x1C}},
  {'E', {0x7F, 0x49, 0x49, 0x49, 0x41}},
  {'G', {0x3E, 0x41, 0x49, 0x49, 0x7A}},
  {'H', {0x7F, 0x08, 0x08, 0x08, 0x7F}},
  {'I', {0x00, 0x41, 0x7F, 0x41, 0x00}},
  {'K', {0x7F, 0x08, 0x14, 0x22, 0x41}},
  {'L', {0x7F, 0x40, 0x40, 0x40, 0x40}},
  {'M', {0x7F, 0x02, 0x0C, 0x02, 0x7F}},
  {'N', {0x7F, 0x02, 0x04, 0x08, 0x7F}},
  {'O', {0x3E, 0x41, 0x41, 0x41, 0x3E}},
  {'P', {0x7F, 0x09, 0x09, 0x09, 0x06}},
  {'R', {0x7F, 0x09, 0x19, 0x29, 0x46}},
  {'S', {0x46, 0x49, 0x49, 0x49, 0x31}},
  {'T', {0x01, 0x01, 0x7F, 0x01, 0x01}},
  {'Y', {0x01, 0x02, 0x7C, 0x02, 0x01}}
};

static I2C_HandleTypeDef *oled_i2c;
static uint16_t oled_address;
static uint8_t framebuffer[OLED_WIDTH * OLED_PAGES];

static const uint8_t *FindGlyph(char character)
{
  for (uint8_t index = 0U; index < (uint8_t)(sizeof(font) / sizeof(font[0]));
       index++)
  {
    if (font[index].character == character)
    {
      return font[index].columns;
    }
  }

  return font[0].columns;
}

static void SetPixel(uint8_t x, uint8_t y)
{
  if ((x < OLED_WIDTH) && (y < OLED_HEIGHT))
  {
    framebuffer[x + ((uint16_t)(y / 8U) * OLED_WIDTH)] |=
      (uint8_t)(1U << (y % 8U));
  }
}

static void DrawChar(uint8_t x, uint8_t y, char character, uint8_t scale)
{
  const uint8_t *glyph = FindGlyph(character);

  for (uint8_t column = 0U; column < 5U; column++)
  {
    for (uint8_t row = 0U; row < 7U; row++)
    {
      if ((glyph[column] & (uint8_t)(1U << row)) != 0U)
      {
        for (uint8_t dx = 0U; dx < scale; dx++)
        {
          for (uint8_t dy = 0U; dy < scale; dy++)
          {
            SetPixel((uint8_t)(x + (column * scale) + dx),
                     (uint8_t)(y + (row * scale) + dy));
          }
        }
      }
    }
  }
}

static void DrawText(uint8_t x, uint8_t y, const char *text, uint8_t scale)
{
  while (*text != '\0')
  {
    DrawChar(x, y, *text, scale);
    x = (uint8_t)(x + (6U * scale));
    text++;
  }
}

static void DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
  int16_t dx = (x1 >= x0) ? (x1 - x0) : (x0 - x1);
  int16_t sx = (x0 < x1) ? 1 : -1;
  int16_t dy = (y1 >= y0) ? (y0 - y1) : (y1 - y0);
  int16_t sy = (y0 < y1) ? 1 : -1;
  int16_t error = dx + dy;

  while (1)
  {
    int16_t doubled_error;

    SetPixel((uint8_t)x0, (uint8_t)y0);
    if ((x0 == x1) && (y0 == y1))
    {
      break;
    }
    doubled_error = (int16_t)(2 * error);
    if (doubled_error >= dy)
    {
      error += dy;
      x0 += sx;
    }
    if (doubled_error <= dx)
    {
      error += dx;
      y0 += sy;
    }
  }
}

static void ClearFigurePanel(void)
{
  for (uint8_t page = 0U; page < OLED_PAGES; page++)
  {
    (void)memset(&framebuffer[((uint16_t)page * OLED_WIDTH) +
                              FIGURE_PANEL_LEFT],
                 0, OLED_WIDTH - FIGURE_PANEL_LEFT);
  }
}

static void DrawFigure(SSD1306_Alert alert, uint8_t frame)
{
  for (uint8_t y = 0U; y < OLED_HEIGHT; y += 2U)
  {
    SetPixel(84U, y);
  }

  DrawLine(104, 3, 108, 3);
  DrawLine(103, 4, 103, 7);
  DrawLine(109, 4, 109, 7);
  DrawLine(104, 8, 108, 8);
  DrawLine(106, 9, 106, 19);

  if (alert == SSD1306_ALERT_SHOCK)
  {
    DrawLine(106, 12, 100, 5);
    DrawLine(106, 12, 112, 5);
    DrawLine(106, 19, 100, 27);
    DrawLine(106, 19, 112, 27);
  }
  else if ((frame & 1U) == 0U)
  {
    DrawLine(106, 12, 100, 17);
    DrawLine(106, 12, 112, 9);
    DrawLine(106, 19, 101, 27);
    DrawLine(106, 19, 111, 25);
  }
  else
  {
    DrawLine(106, 12, 100, 9);
    DrawLine(106, 12, 112, 17);
    DrawLine(106, 19, 101, 25);
    DrawLine(106, 19, 111, 27);
  }

  DrawLine(92, 29, 120, 29);
}

static HAL_StatusTypeDef SendCommands(const uint8_t *commands, uint8_t length)
{
  uint8_t packet[32];

  if (length > (sizeof(packet) - 1U))
  {
    return HAL_ERROR;
  }

  packet[0] = 0x00U;
  (void)memcpy(&packet[1], commands, length);
  return HAL_I2C_Master_Transmit(oled_i2c, oled_address, packet,
                                 (uint16_t)(length + 1U), 100U);
}

HAL_StatusTypeDef SSD1306_SetDisplayEnabled(uint8_t enabled)
{
  const uint8_t command = (enabled != 0U) ? 0xAFU : 0xAEU;

  return SendCommands(&command, 1U);
}

static HAL_StatusTypeDef UpdateScreen(void)
{
  const uint8_t set_window[] = {0x21U, 0x00U, 0x7FU,
                                0x22U, 0x00U, 0x03U};
  uint8_t packet[17];

  if (SendCommands(set_window, sizeof(set_window)) != HAL_OK)
  {
    return HAL_ERROR;
  }

  packet[0] = 0x40U;
  for (uint16_t offset = 0U; offset < sizeof(framebuffer); offset += 16U)
  {
    (void)memcpy(&packet[1], &framebuffer[offset], 16U);
    if (HAL_I2C_Master_Transmit(oled_i2c, oled_address, packet,
                                sizeof(packet), 100U) != HAL_OK)
    {
      return HAL_ERROR;
    }
  }

  return HAL_OK;
}

static HAL_StatusTypeDef UpdateFigurePanel(void)
{
  const uint8_t set_window[] = {0x21U, FIGURE_PANEL_LEFT, 0x7FU,
                                0x22U, 0x00U, 0x03U};
  uint8_t packet[17];

  if (SendCommands(set_window, sizeof(set_window)) != HAL_OK)
  {
    return HAL_ERROR;
  }

  packet[0] = 0x40U;
  for (uint8_t page = 0U; page < OLED_PAGES; page++)
  {
    for (uint8_t x = FIGURE_PANEL_LEFT; x < OLED_WIDTH; x += 16U)
    {
      (void)memcpy(&packet[1],
                   &framebuffer[((uint16_t)page * OLED_WIDTH) + x], 16U);
      if (HAL_I2C_Master_Transmit(oled_i2c, oled_address, packet,
                                  sizeof(packet), 100U) != HAL_OK)
      {
        return HAL_ERROR;
      }
    }
  }

  return HAL_OK;
}

HAL_StatusTypeDef SSD1306_Init(I2C_HandleTypeDef *i2c, uint8_t address)
{
  const uint8_t init_commands[] = {
    0xAEU, 0xD5U, 0x80U, 0xA8U, 0x1FU, 0xD3U, 0x00U, 0x40U,
    0x8DU, 0x14U, 0x20U, 0x00U, 0xA1U, 0xC8U, 0xDAU, 0x02U,
    0x81U, 0x8FU, 0xD9U, 0xF1U, 0xDBU, 0x40U, 0xA4U, 0xA6U,
    0xAFU
  };

  oled_i2c = i2c;
  oled_address = (uint16_t)(address << 1U);
  (void)memset(framebuffer, 0, sizeof(framebuffer));

  if (SendCommands(init_commands, sizeof(init_commands)) != HAL_OK)
  {
    return HAL_ERROR;
  }

  return UpdateScreen();
}

static void FormatCountLine(const char *label, uint16_t count, char *line)
{
  char digits[6];
  uint8_t digit_position = sizeof(digits) - 1U;
  uint8_t line_position = 0U;

  digits[digit_position] = '\0';
  do
  {
    digits[--digit_position] = (char)('0' + (count % 10U));
    count /= 10U;
  } while (count != 0U);

  while (*label != '\0')
  {
    line[line_position++] = *label++;
  }
  line[line_position++] = ' ';
  while (digits[digit_position] != '\0')
  {
    line[line_position++] = digits[digit_position++];
  }
  line[line_position] = '\0';
}

HAL_StatusTypeDef SSD1306_ShowLoggerStatus(const char *temperature_text,
                                           uint16_t shock_count,
                                           uint16_t motion_count,
                                           SSD1306_Alert alert,
                                           uint8_t figure_frame)
{
  char shock_line[12];
  char motion_line[13];
  const char *alert_text = "READY";

  FormatCountLine("SHOCK", shock_count, shock_line);
  FormatCountLine("MOTION", motion_count, motion_line);
  if (alert == SSD1306_ALERT_SHOCK)
  {
    alert_text = "SHOCK!";
  }
  else if (alert == SSD1306_ALERT_MOTION)
  {
    alert_text = "MOTION!";
  }

  (void)memset(framebuffer, 0, sizeof(framebuffer));
  DrawText(0U, 0U, "TEMP", 1U);
  DrawText(36U, 0U, temperature_text, 1U);
  DrawText(0U, 8U, shock_line, 1U);
  DrawText(0U, 16U, motion_line, 1U);
  DrawText(0U, 24U, alert_text, 1U);
  DrawFigure(alert, figure_frame);
  return UpdateScreen();
}

HAL_StatusTypeDef SSD1306_UpdateFigure(SSD1306_Alert alert,
                                       uint8_t figure_frame)
{
  ClearFigurePanel();
  DrawFigure(alert, figure_frame);
  return UpdateFigurePanel();
}
