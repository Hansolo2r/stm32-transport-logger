# BLE dashboard

This static page connects to the JDY-16 serial module through Web Bluetooth.
It is deployed at
[hansolo2r.github.io/stm32-transport-logger](https://hansolo2r.github.io/stm32-transport-logger/).

## Use on iPhone

1. Open the HTTPS page in Bluefy. Safari does not expose Web Bluetooth.
2. Select `JDY-16` after pressing the connect button.
3. Read the live temperature, RTC time, motion state, event counts, storage
   state, and XYZ acceleration.
4. Use the record and event views for chronological history.
5. Synchronize the DS1302 clock or adjust detector parameters when needed.

The current Bluefy workflow can operate the dashboard, but its Blob download
does not save the CSV file. The archived data in this repository was exported
through USART1.

## Device interface

- BLE service: `FFE0`
- BLE characteristic: `FFE1`
- STM32 interface: USART2 on PA2/PA3 at 9600 baud, 8N1
- Status polling: `status_json` every three seconds while active

Supported controls include `time`, `time set`, `preview`, `events`, `pause`,
`resume`, `sleep`, `wake`, configuration read/write, and CSV export commands.
Long commands are split into BLE writes of at most 20 bytes.

## Local checks

See [../build-and-test.md](../build-and-test.md) for the Node and Playwright
commands. The page has no build step.
