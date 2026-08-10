# Hardware Bill of Materials

This list reflects the physically assembled prototype and the development tools
confirmed in the project evidence. Purchase prices are intentionally omitted
because receipts and exact per-item costs were not archived.

## Device Assembly

| Item | Quantity | Confirmed model or description | Role / connection |
| --- | ---: | --- | --- |
| Microcontroller board | 1 | STM32F103C8T6 core board | Main controller |
| Temperature module | 1 | DS18B20 module | DQ on PA8, 3.3 V |
| Display | 1 | 0.91-inch 128x32 SSD1306 OLED | I2C1 PB8/PB9, address 0x3C |
| Accelerometer | 1 | GY-291 ADXL345 | I2C2 PB10/PB11, address 0x53 |
| Flash module | 1 | W25Q64, 8 MB | SPI2 PB12-PB15 |
| Real-time clock | 1 | DS1302 module with coin cell | CLK/DAT/RST on PA4/PA5/PA6 |
| RTC backup cell | 1 | CR2032 | Maintains DS1302 time |
| BLE module | 1 | JDY-16 | USART2 PA2/PA3 at 9600 baud |
| Prototype carrier | 1 | 830-hole solderless breadboard | Mechanical placement and signal distribution |
| Battery | 1 | 3.7 V 400 mAh LiPo pack | Portable power source |
| Charger | 1 | Small-current single-cell LiPo Type-C charging module | Charges the 3.7 V LiPo; exact controller not documented |
| Regulator | 1 | TPS63020 buck-boost module | Regulated 3.3 V power rail |
| Power switch | 1 | SS12D10 three-pin two-position slide switch | Disconnects the load from the battery power chain |
| Wiring | as required | Dupont jumpers and insulated hookup wire | Signal and power connections |

## Development-Only Tools

| Item | Quantity | Confirmed model or description | Role |
| --- | ---: | --- | --- |
| Programmer/debugger | 1 | ST-Link V2+ | SWD firmware download and verification |
| USB-to-TTL adapter | 1 | UST-343A | USART1 diagnostics and CSV export at 115200 baud |
| Multimeter | 1 | Model not recorded | Continuity and 3.3 V rail checks |
| Computer | 1 | Windows PC | STM32CubeMX, STM32CubeIDE, serial terminal, and repository work |
| Phone | 1 | iPhone | Bluefy Web Bluetooth dashboard host |

## Evidence Boundary

The BOM records what was used, not a procurement recommendation. Exact charger
controller, vendor SKUs, prices, regulator efficiency, and battery capacity
verification were not independently documented. Do not infer those values from
generic module appearance.
