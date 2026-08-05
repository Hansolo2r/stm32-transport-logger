# Breadboard Enclosure Layout Design

## Objective

Create a practical, beginner-readable assembly and wiring reference for mounting the STM32 transport logger on one 830-hole breadboard. The final reference must use the real module sizes, preserve the currently working firmware pin map, and leave the external controls and interfaces accessible.

## Selected Layout

Use the approved front-mounted, zone-separated layout (scheme A) with the breadboard in landscape orientation:

- Left power zone: charger at the left edge with its Type-C connector facing outward, SS12D10 switch at the left edge with its lever facing outward, TPS63020 beside the switch, and the 40 x 20 x 6 mm battery above them.
- Center processing zone: STM32F103C8T6 mounted horizontally with its USB-C connector facing left. Keep reset, SWD, and USART1 access unobstructed.
- Center measurement zone: ADXL345 mounted flat and rigidly as close to the assembled device's geometric center as practical.
- SPI storage zone: W25Q64 placed close to the STM32 PB12-PB15 side.
- Right interface zone: DS1302 and OLED placed with the coin cell and display face accessible.
- Far-right wireless and cold zone: JDY-16 antenna faces outward with no battery, metal, or dense wiring in front of or underneath the antenna. The DS18B20 module is placed at the opposite edge from the power zone, with its sensor head exposed to ambient air.

The battery stays on the front of the breadboard. Moving it to the rear is only a fallback if the real modules cannot be fixed without overlap.

## Power Architecture

The intended power chain is:

`3.7 V LiPo -> low-current charger -> SS12D10 switch -> TPS63020 -> regulated 3.3 V rails`

- Configure the TPS63020 output to 3.3 V and verify it with a multimeter before connecting the STM32.
- Use the SS12D10 center pin and one outside pin. Leave the remaining outside pin disconnected. Verify the ON position with continuity mode.
- The charger module's exact battery and load pads must be copied from its real silkscreen after arrival. Do not infer pad order from a product photograph.
- If the charger has `B+/B-` and `OUT+/OUT-`, connect the battery to B and the switch/TPS input to OUT.
- If the charger has no separate OUT pads, place the switch in the positive line between the charger/battery positive node and TPS input positive.
- Connect all grounds directly; only switch the positive supply line.
- Bridge the left and right halves of every used breadboard power rail. Bridge the top 3.3 V rail to the bottom 3.3 V rail and the top GND rail to the bottom GND rail.
- Supply the STM32 through its 3.3 V pin, not its 5 V pin.

## Firmware-Matched Signal Wiring

| Module | Module pins | Destination |
| --- | --- | --- |
| DS18B20 | VCC, GND, DQ | 3.3 V, GND, PA8 |
| SSD1306 OLED | VCC, GND, SCL, SDA | 3.3 V, GND, PB8, PB9 |
| ADXL345 | VCC, GND, SCL, SDA, CS, SDO | 3.3 V, GND, PB10, PB11, 3.3 V, GND |
| W25Q64 | VCC, GND, CS, CLK, DO, DI | 3.3 V, GND, PB12, PB13, PB14, PB15 |
| DS1302 | VCC, GND, CLK, DAT, RST | 3.3 V, GND, PA4, PA5, PA6 |
| JDY-16 | VCC, GND, RX, TX | 3.3 V, GND, PA2 (TX), PA3 (RX) |

External debug connections remain temporary:

- ST-Link: SWDIO to PA13, SWCLK to PA14, and GND to GND.
- UST-343A: TX to PA10, RX to PA9, and GND to GND.
- Do not connect either adapter's VCC while the logger is battery powered.

## Mechanical Rules

- Mount the ADXL345 flat and rigidly; loose foam suspension would corrupt motion and shock measurements.
- Do not cover the JDY-16 antenna with the battery, copper tape, metal fasteners, or dense wire bundles.
- Keep the DS18B20 away from the charger, TPS63020, STM32 regulator area, and battery.
- Keep the OLED face, charger Type-C port, switch lever, reset button, and DS1302 coin cell accessible.
- Use insulating foam tape or plastic spacers under modules that cannot be inserted directly. Adhesive must not cover ICs, antenna areas, the temperature sensor head, connectors, or battery surfaces.
- Route wires along the breadboard edges and rails where practical. Do not run wires over the OLED face, switch lever, reset button, or BLE antenna.

## Final Visual Deliverables

1. A real-size placement diagram based on the user's actual 830-hole breadboard, with every current and incoming module labeled and oriented.
2. A color-coded wiring diagram showing each signal, 3.3 V distribution, GND distribution, rail bridges, switch wiring, and the charger/TPS power chain.
3. A staged assembly and test diagram that adds one subsystem at a time.

The charger pads remain explicitly conditional until a clear photograph of the received module and its silkscreen is available.

## Validation

Before permanent fixing:

1. Test the SS12D10 with continuity mode.
2. Power only the charger, switch, and TPS63020; verify polarity and 3.3 V output.
3. Add the STM32 and verify boot/status LED.
4. Add OLED, DS18B20, ADXL345, W25Q64, DS1302, and JDY-16 one at a time.
5. After each module, confirm the existing serial diagnostics and mobile interface still work.
6. Perform a full power-off/restart test, Flash export test, RTC continuity test, BLE reconnect test, and controlled motion/shock test before closing the enclosure.

## Safety Boundary

Never connect battery power, STM32 USB-C power, ST-Link 3.3 V, and USB-UART VCC simultaneously. During battery-powered debugging, use signal and GND connections only. Stop immediately if the DS1302, battery, charger, or TPS63020 becomes hot.
