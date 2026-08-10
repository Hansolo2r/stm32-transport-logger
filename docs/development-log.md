# Development notes

This file keeps the main engineering decisions and observed failures. Routine
editing history is omitted.

## 2026-07-31: controller and sensor bring-up

- Configured the STM32F103C8T6 project in CubeMX and used the generated HAL
  project as the hardware configuration source.
- Brought up the PC13 status LED, DS18B20, SSD1306, and ADXL345.
- Kept physical observations separate from successful compilation.

## 2026-08-01 to 2026-08-03: persistent logging

- Added the W25Q64 driver and checked JEDEC identification, program/readback,
  and power-cycle retention.
- Implemented 32-byte CRC-protected records in sixteen 4 KB sectors. The
  circular region holds 2,048 records.
- Added serial status, preview, raw export, simplified CSV export, and guarded
  log clearing through `clear confirm`.

## 2026-08-04: BLE dashboard

- Added JDY-16 communication on USART2 while retaining USART1 diagnostics.
- Added phone views for status, temperature history, motion, impacts, and
  parameter settings.
- Deployed the static dashboard through GitHub Pages. On iOS it is opened in
  Bluefy because Safari does not provide Web Bluetooth.

## 2026-08-05: RTC timestamps and wiring references

- Integrated the DS1302 and added calendar timestamps to new records, status
  messages, and export formats.
- Preserved compatibility with older records that do not contain RTC data.
- Added breadboard placement, power, and signal drawings as assembly guidance.
  These drawings are not electrical test evidence.

## 2026-08-08: motion and impact classification

- Replaced one-sample decisions with a two-timescale state machine. Short
  high-amplitude pulses are treated separately from sustained motion.
- Changed motion evidence from total acceleration magnitude alone to
  three-axis changes, which also detects orientation changes near 1 g.
- Replaced consecutive-sample stillness with a bounded candidate resting
  region. This fixed a hardware observation where occasional sensor jitter
  kept the state in motion after the device was placed down.
- The final host suite contains 15 synthetic scenarios. The final firmware was
  flashed, and hand-held checks confirmed motion start, return to static, a
  light impact at rest, and a light impact while moving.

## 2026-08-10: data archive

- Exported 420 sequential engineering records and 388 simplified records from
  W25Q64 over USART1.
- A separate SecureCRT capture contains 378 rows that match the first 378 rows
  of the later simplified export.
- The data came from development handling without synchronized labels. It
  supports storage and export claims, not detection accuracy or transport
  performance.
- Detector thresholds and timing can be changed and saved. Testing for other
  mounting conditions, cold-chain transport, or vehicle impacts was not
  performed in this project.

## Checks not performed

- Battery endurance under a documented workload
- RTC retention during a measured main-power-off interval
- Temperature comparison with a reference thermometer
- Enclosure-level calibration and labelled transport trials
- Direct CSV file saving in the current iOS Bluefy workflow
