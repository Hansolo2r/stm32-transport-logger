# Development Log

## 2026-08-03 - Hardware identification and DS18B20 integration prepared

- Objective: identify the received modules and prepare the first product milestone, ambient temperature acquisition.
- Hardware identified from photos: W25Q64JV 8 MB SPI Flash module; 0.91-inch four-pin I2C OLED, expected SSD1306 128x32; GY-291 ADXL345 accelerometer; three-pin DS18B20 module.
- Wiring selected: DS18B20 `VCC -> 3.3V`, `GND -> GND`, `DQ -> PA8`.
- CubeMX: PA8 configured as `GPIO_Output`, open-drain, no internal pull, high initial level, label `DS18B20_DQ`.
- Firmware: added a 1-Wire driver with presence detection and scratchpad CRC validation. `main.c` starts conversion without a 750 ms blocking delay and reports a sample over USART1 about every two seconds.
- Build validation: STM32CubeIDE Debug clean build completed with 0 errors and 0 warnings. Firmware uses 23,780 bytes of code/constant Flash, 12 bytes of initialized data, and 2,000 bytes of zero-initialized RAM.
- Hardware validation status: wiring and board test are still required.
- Expected USART1 output: `DS18B20: +025.3 C`; disconnected sensor: `DS18B20: sensor not found`.
- Limitation: OLED controller and resolution are inferred from module form factor and must be confirmed by I2C scan/display test.
- Next milestone: verify room-temperature stability, hand-warming response, disconnect detection, and a 30-minute run before adding the OLED.

## 2026-08-03 - Legacy exercise cleanup

- Objective: turn the accumulated learning project into a clean base for the temperature and motion logger.
- Removed hardware configurations: 4x4 keypad, USART2/JDY-16, PA4 ADC exercise input, PB0 TIM3 breathing LED, TIM2 stopwatch interrupt, and TM1650 display/key interface.
- Freed GPIOs: PA0, PA1, PA2, PA3, PA4, PB0, PB1, PB6, PB7, PB8, PB9, PB10, PB11, and PB12.
- Removed application features: stopwatch/countdown, keypad commands, Bluetooth command forwarding, ADC streaming, internal-Flash setting storage, TM1650 display and key handling, and external PWM LED control.
- Deleted files: `Core/Src/tm1650.c` and `Core/Inc/tm1650.h`.
- Generation safety: moved application behavior into `Core/Src/app.c`; `main.c` calls it only from CubeMX-preserved `USER CODE` regions so future code generation keeps the logger logic.
- Retained hardware: DS18B20 on PA8, USART1 on PA9/PA10, status LED on PC13, SWD on PA13/PA14, and SPI2 on PB13/PB14/PB15 for the purchased W25Q64.
- Build validation: STM32CubeIDE Debug clean build completed with 0 errors and 0 warnings. Final generation-safe firmware uses 8,468 bytes of code/constant Flash and 1,764 bytes of zero-initialized RAM, down from 23,780 and 2,000 bytes respectively.
- Expected behavior: PC13 blinks every 500 ms and USART1 reports the DS18B20 temperature about every two seconds. No TM1650, keypad, ADC, Bluetooth, stopwatch, or PWM messages remain.
- Next milestone: complete physical DS18B20 tests, then configure PB8/PB9 as the shared I2C bus for OLED and ADXL345.

## 2026-08-03 - DS18B20 disconnected-data validation

- Observed failure: disconnecting the DQ wire produced `0.0 C` instead of an error.
- Root cause: the module carries the 1-Wire pull-up resistor. Removing the module leaves PA8 floating; an all-zero scratchpad also has a numerically valid zero CRC, so CRC alone cannot identify this disconnect pattern.
- Fix: reject all-zero and all-one scratchpads, keep CRC validation, and reject temperatures outside the DS18B20 range of -55 to 125 degrees Celsius.
- User-visible result: invalid data now reports `DS18B20: disconnected or data error` instead of a false zero-degree measurement.

## 2026-08-03 - OLED I2C bus bring-up prepared

- Objective: verify the OLED wiring and real I2C address before adding an SSD1306 display driver.
- Wiring: OLED `GND -> GND`, `VCC -> 3.3V`, `SCL -> PB8`, and `SDA -> PB9`.
- CubeMX: enabled I2C1 at 100 kHz using the STM32F103 remapped pins PB8/PB9.
- Firmware: added a startup scan of all valid 7-bit I2C addresses using `HAL_I2C_IsDeviceReady`; found addresses are printed over USART1.
- Build issue and fix: the first build failed because the legacy HAL configuration still had `HAL_I2C_MODULE_ENABLED` commented out. Enabling the module restored the I2C handle type, constants, and HAL API declarations.
- Build validation: the corrected STM32CubeIDE Debug clean build completed with 0 errors and 0 warnings; firmware uses 10,648 bytes of code/constant Flash and 1,852 bytes of zero-initialized RAM.
- Expected result for the photographed OLED: normally `I2C: device at 0x3C`; the scan is treated as authoritative because the controller is hidden under the display.
- Next milestone: after a confirmed address, add the 128x32 framebuffer and SSD1306 initialization/display code.

## 2026-08-03 - SSD1306 temperature display

- Observed result from hardware: the I2C scan completed normally and found the OLED device.
- Firmware: added a local SSD1306 128x32 driver with a 512-byte framebuffer, initialization sequence, compact glyph set, scaled text drawing, and chunked I2C screen updates.
- UI: the first line shows `TEMP LOGGER`; the second line shows the current DS18B20 value such as `+025.3 C` using 2x characters.
- Integration: the display initializes only when address 0x3C or 0x3D is discovered, and refreshes only after a validated temperature sample.
- Failure handling: I2C initialization or refresh failures are reported over USART1 and disable further display updates without stopping temperature acquisition.
- Build validation: STM32CubeIDE Debug clean build completed with 0 errors and 0 warnings; firmware uses 13,692 bytes of code/constant Flash and 2,372 bytes of zero-initialized RAM.
- Hardware result: the OLED initialized and displayed the live DS18B20 temperature correctly.

## 2026-08-03 - Human-readable temperature formatting

- Requirement: remove the explicit plus sign and unnecessary leading zeroes; show 27 degrees as `27.0 C` instead of `+027.0 C`.
- Firmware: application code now formats one shared temperature string for both USART1 and OLED output. Negative values retain the minus sign, and the OLED centers strings of different lengths dynamically.

## 2026-08-03 - ADXL345 data acquisition prepared

- Objective: add three-axis acceleration acquisition before implementing movement, tilt, and shock event rules.
- Wiring plan: GY-291 `GND -> GND`, `VCC -> 3.3V`, `SCL -> PB8`, `SDA -> PB9`, `CS -> 3.3V`, and `SDO -> GND`; INT1/INT2 remain disconnected for polling tests.
- Bus design: OLED and ADXL345 share PB8/PB9; their different I2C addresses allow the MCU to select each device independently.
- Firmware: added ADXL345 device-ID validation, full-resolution +/-2 g mode, 100 Hz output data rate, measurement mode, six-byte XYZ reads, and approximate milligravity conversion.
- Expected scan result after wiring: OLED at 0x3C and ADXL345 at 0x53. With the board lying flat, two axes should be near 0 mg and the gravity-aligned axis should be near +1000 or -1000 mg.
- Build validation: STM32CubeIDE Debug clean build completed with 0 errors and 0 warnings; firmware uses 17,376 bytes of code/constant Flash and 2,380 bytes of zero-initialized RAM.

## 2026-08-03 - ADXL345 moved to a dedicated I2C bus

- Design decision: the user preferred separate physical buses for the display and accelerometer, so the earlier shared-bus wiring plan was superseded.
- CubeMX: enabled I2C2 at 100 kHz on PB10/PB11. I2C1 remains at 100 kHz on remapped PB8/PB9 for the OLED.
- Final wiring: OLED `SCL -> PB8`, `SDA -> PB9`; ADXL345 `SCL -> PB10`, `SDA -> PB11`, `CS -> 3.3V`, and `SDO -> GND`.
- Firmware: added the I2C2 handle, initialization, GPIO alternate-function open-drain setup, and peripheral clock setup. The application now scans I2C1 for the OLED and I2C2 for the ADXL345 independently.
- Build validation: STM32CubeIDE Debug clean build completed in an isolated headless workspace with 0 errors and 0 warnings. Firmware uses 17,652 bytes of code/constant Flash, 12 bytes of initialized data, and 2,468 bytes of zero-initialized RAM.
- Hardware validation pending: USART1 should find the OLED on `I2C1 PB8/PB9` and the accelerometer at 0x53 on `I2C2 PB10/PB11`.

## 2026-08-03 - Orientation test and first motion-event detector

- Hardware result: the ADXL345 passed a six-face test; each positive and negative axis produced approximately +1000 mg or -1000 mg while the other axes remained near zero.
- Sensor configuration: changed from +/-2 g to full-resolution +/-8 g so impacts above 2 g can be measured instead of immediately saturating.
- Sampling: accelerometer reads now run every 20 ms (50 Hz), while routine XYZ output remains at 1 Hz to keep USART output readable.
- Event rules: three consecutive samples outside 0.8-1.2 g total magnitude start a motion event; one second inside 0.85-1.15 g ends it; total magnitude above 2.5 g records a shock with a 500 ms repeat guard.
- Implementation note: comparisons use squared XYZ magnitude, avoiding an unnecessary square-root operation on the microcontroller.
- Build validation: STM32CubeIDE Debug clean build completed with 0 errors and 0 warnings. Firmware uses 18,440 bytes of code/constant Flash, 12 bytes of initialized data, and 2,484 bytes of zero-initialized RAM.
- Limitation: these are initial engineering thresholds, not calibrated transport standards. They require controlled hand-motion and tap tests before being treated as useful event definitions.

## 2026-08-03 - Shock sensitivity adjustment

- Observed result: ordinary controlled taps did not trigger the initial shock rule.
- Likely causes: the 2.5 g threshold was too high for safe bench testing, and 50 Hz polling could miss a short acceleration peak between samples.
- Adjustment: increased accelerometer polling from 50 Hz to 100 Hz and lowered the initial shock threshold from 2.5 g to 1.6 g.
- Timing preservation: motion confirmation changed from 3 to 6 samples and stillness confirmation from 50 to 100 samples, preserving approximately 60 ms and 1 second respectively at the higher sampling rate.
- Validation pending: repeat controlled hand-jolt and desk-tap tests; reduce the threshold further only if 100 Hz/1.6 g still misses events, and check false triggers during normal handling.
- Hardware result: after downloading the adjusted firmware, controlled motion and shock triggering worked approximately as intended. The 100 Hz/1.6 g settings are retained as the first usable bench-test baseline; formal threshold calibration remains future work.

## 2026-08-03 - W25Q64 SPI identity test prepared

- Objective: bring up the external nonvolatile memory using a read-only identity command before any erase or write operation.
- Wiring: W25Q64 module `VCC -> 3.3V`, `GND -> GND`, `CS -> PB12`, `SCK -> PB13`, `DO -> PB14`, and `DI -> PB15`.
- CubeMX: retained SPI2 mode 0 at 12 Mbit/s on PB13/PB14/PB15 and configured PB12 as push-pull `FLASH_CS`, initially high.
- Firmware: added a small W25Q64 driver that asserts CS, sends JEDEC command 0x9F, reads three identification bytes, rejects all-zero/all-one responses, and checks for the expected `EF 40 17` W25Q64 ID.
- Safety boundary: this milestone performs no erase or program command, so it cannot intentionally alter Flash contents.
- Build validation: STM32CubeIDE Debug clean build completed with 0 errors and 0 warnings. Firmware uses 20,308 bytes of code/constant Flash, 12 bytes of initialized data, and 2,484 bytes of zero-initialized RAM.
- Hardware validation pending: expected USART1 output is `SPI2 W25Q64 JEDEC ID: EF 40 17` followed by `W25Q64: 8 MB flash detected`.

## 2026-08-03 - ADXL345 transient I2C recovery

- Observed failure: after operating normally for a period, one I2C read error permanently stopped accelerometer output.
- Software root cause: the first failed `HAL_I2C_Mem_Read` immediately cleared the application-ready flag, with no retry or recovery path.
- Fix: tolerate up to four consecutive read failures; after five, report a persistent fault, reinitialize I2C2 and the ADXL345 once per second, and automatically resume sampling after communication recovers.
- Diagnostic correction: changed the startup message from 50 Hz to the actual 100 Hz polling rate.
- Physical limitation: recovery handles transient software/peripheral errors but cannot repair reversed wiring, an unpowered module, a sustained loose connection, or missing I2C pull-ups.

## 2026-08-03 - W25Q64 erase, program, readback, and retention test prepared

- Reserved region: the last 4 KB sector at address `0x7FF000` is exclusively assigned to bring-up testing and excluded from future production logs.
- First boot behavior: if no valid marker exists, erase only the reserved sector, program `LOGGER_FLASH_TEST_V1`, read it back, and compare every byte.
- Later boot behavior: when the marker is already present, perform a read-only comparison and report that it survived reset or power loss; do not erase or program again.
- Driver additions: write-enable, status-register polling, 4 KB sector erase, page program with 256-byte/page-boundary checks, normal data read, capacity-bound checks, and operation timeouts.
- Build validation: STM32CubeIDE Debug clean build completed with 0 errors and 0 warnings. Firmware uses 23,840 bytes of code/constant Flash, 12 bytes of initialized data, and 2,500 bytes of zero-initialized RAM.
- Hardware validation pending: first confirm `marker written and verified`, then remove all board power, reconnect it, and confirm `marker retained after reset/power loss`.

## 2026-08-03 - W25Q64 no-response diagnostics

- Observed failure: after previously successful JEDEC identification, a later firmware run reported no valid SPI response.
- Diagnostic improvement: raw JEDEC bytes are now always printed, including all-zero and all-one responses, so open-circuit, stuck-low, and unstable-bus symptoms can be distinguished.
- Signal-integrity adjustment: reduced SPI2 from 12 Mbit/s to 3 Mbit/s for the temporary breadboard and Dupont-wire assembly; this remains far faster than the logger requires.
- Hardware validation pending: classify the returned ID as `EF 40 17`, `FF FF FF`, `00 00 00`, or unstable/random before changing wiring or driver logic further.
- Observed raw response: `FF FF FF`, indicating that PB14/MISO remained high instead of receiving driven data from the Flash.
- Software exclusion step: added the standard `0xAB` release-from-power-down command before JEDEC identification. If the result remains all ones, investigation should focus on VCC/GND, CS, SCK, DO continuity, soldering, and loose breadboard contacts.
- Final hardware result: after connection correction and the diagnostic firmware, JEDEC identification returned `EF 40 17`. The reserved marker was also read successfully after reset/power loss, confirming SPI communication, write/read verification, and nonvolatile retention.
- One startup capture contained a partial boot followed by a complete boot. This is consistent with a manual/download reset if it occurred only once; repeated spontaneous boots would require separate power/reset investigation.

## 2026-08-03 - First persistent structured event log

- Scope: use the first 4 KB Flash sector for an intentionally limited first logger implementation; retain the final 4 KB sector for bring-up tests.
- Record format: fixed 32-byte little-endian records with `TLG1` magic, format version, event type, sequence number, boot-relative seconds, temperature in tenths of a degree, XYZ acceleration in mg, and CRC-16/CCITT.
- Stored events: one boot record per reset, the first valid temperature followed by one temperature record every 60 seconds, plus immediate motion-start, motion-end, and shock records.
- Startup behavior: scan up to 128 records, validate magic/version/length/CRC, stop at the first erased slot, recover the next sequence and append address, and continue without erasing previous records.
- Write integrity: each 32-byte page program is read back and compared byte-for-byte; any failure disables further logging for that boot to protect existing records.
- Capacity boundary: this milestone intentionally stops after 128 records and never automatically erases or wraps. Multi-sector append and circular retention remain future work.
- Build validation: STM32CubeIDE Debug clean build completed with 0 errors and 0 warnings. Firmware uses 25,636 bytes of code/constant Flash, 12 bytes of initialized data, and 2,524 bytes of zero-initialized RAM.
- Hardware validation pending: confirm record count persistence across resets, a 60-second temperature increment, and event-triggered count increments before expanding capacity.
## 2026-08-03 - OLED live logger dashboard

- UI: replaced the temperature-only screen with four compact rows for live temperature, shock count, motion-start count, and current event state.
- Event feedback: a detected shock shows `SHOCK!`; a newly detected motion period shows `MOTION!`. The alert remains visible for about two seconds and then returns to `READY`.
- Refresh policy: the 512-byte OLED framebuffer is transmitted only when temperature, event count, or alert state changes, avoiding continuous I2C traffic in the 100 Hz motion-sampling loop.
- Failure behavior: a disconnected or invalid DS18B20 now changes the OLED temperature field to `--.- C` instead of leaving the previous valid value visible.
- Build validation: STM32CubeIDE Debug clean build completed with 0 errors and 0 warnings; firmware uses 26,364 bytes of code/constant Flash, 12 bytes of initialized data, and 2,548 bytes of zero-initialized RAM.
- Limitation: the two displayed counters currently cover only the present power-on session. Flash records survive power loss, but restoring lifetime counters from those records is reserved for the later log-query/export milestone.

## 2026-08-03 - OLED animated status figure

- UI: used the previously empty right side of the 128x32 OLED for a small animated stick figure, separated from the measurements by a dotted vertical line.
- Animation: two walking poses alternate every 500 ms; during a shock alert the figure switches to a raised-arm impact pose.
- Timing design: animation updates transmit only columns 80-127 instead of the entire 512-byte framebuffer, reducing blocking I2C traffic while ADXL345 motion sampling continues on its separate I2C2 bus.
- CubeMX impact: none; this is display logic only and does not alter pins, clocks, or peripheral initialization.
- Build validation: STM32CubeIDE Debug clean build completed with 0 errors and 0 warnings; firmware uses 27,400 bytes of code/constant Flash, 12 bytes of initialized data, and 2,548 bytes of zero-initialized RAM.
- First hardware-test failure: after downloading, the OLED remained blank and normal application output stopped. The line routine changed its error accumulator before completing both coordinate decisions, so several diagonal limb segments never reached their endpoint and trapped the MCU in the drawing loop.
- Fix: preserve one doubled-error snapshot per Bresenham iteration and use it for both the X and Y decisions. A host-side coordinate simulation then confirmed that every figure segment reaches its requested endpoint in at most 29 pixel steps.

## 2026-08-03 - Persistent event counters

- Objective: make the OLED shock and motion counters survive reset and complete power loss instead of restarting from zero.
- Implementation: extended the existing CRC-validated W25Q64 startup scan to count `MOTION_START` and `SHOCK` records while locating the next append address.
- Startup behavior: after Flash validation, the application restores both counters before appending the new boot record and reports the recovered totals over USART1.
- Data safety: startup remains read-only for the existing records; no sector erase or record rewrite was added.
- Scope: counts represent all valid events in the current 4 KB log sector. The sector currently holds at most 128 fixed-size records and still has no automatic wrap or erase policy.
- Build validation: STM32CubeIDE Debug clean build completed with 0 errors and 0 warnings; firmware uses 27,588 bytes of code/constant Flash, 12 bytes of initialized data, and 2,548 bytes of zero-initialized RAM.

## 2026-08-03 - OLED cold-start recovery

- Observed failure: after complete power removal the OLED stayed blank, but pressing reset after power stabilized made it work immediately.
- Root cause: the STM32 began its first I2C address scan before the OLED module completed power-on startup, so address `0x3C` was missed. This was a startup timing issue, not damaged wiring or a display-driver fault.
- Fix: wait 200 ms once after power-up, then scan and initialize the OLED. If that attempt still fails, wait another 200 ms and automatically scan and initialize once more.
- CubeMX impact: none; PB8/PB9 and the I2C1 100 kHz configuration are unchanged.
- Verification plan: completely remove board power, reconnect it without pressing reset, and confirm the OLED initializes and restores persisted event counters.
- Build validation: STM32CubeIDE Debug clean build completed with 0 errors and 0 warnings; firmware uses 27,772 bytes of code/constant Flash, 12 bytes of initialized data, and 2,548 bytes of zero-initialized RAM.

## 2026-08-03 - USART1 command console and CSV export

- Objective: change USART1 from output-only telemetry into a bidirectional maintenance console for inspecting the logger without reflashing firmware.
- CubeMX: enabled the USART1 global interrupt at NVIC priority 0/0; PA9/PA10 and 115200 baud remain unchanged.
- Receive design: one-byte interrupt reception feeds a 64-byte ring buffer; command parsing and all responses remain in the main loop rather than inside the interrupt handler.
- Commands: `help` lists commands, `status` reports live temperature/event/storage state, and `export` reads every CRC-validated W25Q64 record and emits CSV between `CSV_BEGIN` and `CSV_END` markers.
- Data safety: all commands are read-only. No erase command is exposed in this milestone.
- Limitation: CSV export is synchronous and temporarily pauses normal sampling while at most 128 records are transmitted, expected to take roughly one second at 115200 baud.
- Build validation: STM32CubeIDE Debug clean build completed with 0 errors and 0 warnings; firmware uses 32,744 bytes of code/constant Flash, 12 bytes of initialized data, and 2,636 bytes of zero-initialized RAM.

## 2026-08-03 - Full-log read/write state separation

- Observed failure: after the first 4 KB log sector reached 128 records, appending the next boot record failed and cleared the single Flash-ready flag. The `export` command then incorrectly reported Flash unavailable even though all existing records remained readable.
- Root cause: one state variable represented two different capabilities: appending new records and reading validated historical records.
- Fix: introduced separate read-ready and write-ready states. A full sector now disables appending only; `status` reports read and write state separately, and `export` remains available.
- Full-sector startup: when 128 valid records are found, firmware no longer attempts a guaranteed-to-fail boot append and explicitly reports that records can still be exported.
- Data safety: no erase or overwrite operation was added; the existing 128 records remain unchanged.
- Build validation: STM32CubeIDE Debug clean build completed with 0 errors and 0 warnings; firmware uses 33,080 bytes of code/constant Flash, 12 bytes of initialized data, and 2,644 bytes of zero-initialized RAM.

## 2026-08-03 - User-facing and raw CSV views

- Usability finding: the first CSV exposed internal sequence/type fields and XYZ acceleration evidence, which was useful for engineering diagnostics but unnecessarily complex for routine inspection.
- Default `export`: now emits only `session,time_s,temp_c,motion,shock`, skips boot rows, labels movement as `START`/`END`, and labels shocks as `YES`.
- Diagnostic `export_raw`: preserves the original sequence, record type, uptime, temperature, and XYZ fields for threshold analysis and reproducibility evidence.
- Time limitation: without an RTC, `time_s` is uptime within a power session rather than calendar time. The `session` column separates reboots and prevents different uptime-zero points from being silently mixed.
- Session display refinement: `BOOT` remains an internal power-cycle delimiter and is visible only in `export_raw`. Default `export` omits empty power sessions and assigns a session number only when the session contains a temperature, motion, or shock record. This removes `NA` placeholder rows and keeps the user-facing session numbers continuous without replacing missing measurements with a misleading zero.
- Build validation: STM32CubeIDE Debug clean build completed with 0 errors and 0 warnings; firmware uses 33,520 bytes of code/constant Flash, 12 bytes of initialized data, and 2,644 bytes of zero-initialized RAM.

## 2026-08-03 - Multi-sector circular logging

- Storage layout: expanded the log from one 4 KB sector to the first sixteen W25Q64 sectors (64 KB total), while leaving the reserved persistence-test sector at `0x7FF000` untouched.
- Capacity: increased from 128 to 2,048 records without changing the 32-byte record format, so existing records in sector zero remain readable.
- Circular policy: after all sixteen sectors have been used, the firmware erases the oldest complete sector and continues writing there. Flash erase granularity means 128 oldest records are retired together.
- Startup recovery: scans the complete log region, validates every non-erased record with its existing header and CRC, restores the oldest/newest sequence positions, and resumes after the newest valid record.
- Export behavior: `export` and `export_raw` read records in chronological order even when the physical write position has wrapped to the start of Flash.
- Build validation: STM32CubeIDE Debug clean build completed with 0 errors and 0 warnings; firmware uses 33,636 bytes of code/constant Flash, 12 bytes of initialized data, and 2,652 bytes of zero-initialized RAM before removal of the obsolete single-sector full-state API.
- Hardware validation: the upgraded firmware was downloaded to the STM32F103C8T6 and the user confirmed normal operation with the previously full first sector, demonstrating that logging can continue beyond the former 128-record limit without erasing the existing records.
- Next reliability test: allow at least one new temperature record, power-cycle the board, then verify that startup restores the increased record count and appends a new boot delimiter.
- Power-cycle/export validation: the user confirmed that the retained records exported correctly after the multi-sector upgrade and restart.

## 2026-08-03 - Confirmed log clear command

- Product need: a new trip or experiment must be able to start without mixing old bench-test records into the new dataset.
- Command safety: `clear` only explains the consequence; destructive erasure requires the exact command `clear confirm`.
- Erase boundary: the command erases only the sixteen 4 KB log sectors at the start of W25Q64. The reserved persistence-test sector at `0x7FF000` is not touched.
- Post-clear state: event counters and circular-log state reset, then one internal boot delimiter is written to establish the new session. Default `export` continues to hide that delimiter.
- Build validation: STM32CubeIDE Debug clean build completed with 0 errors and 0 warnings; firmware uses 34,264 bytes of code/constant Flash, 12 bytes of initialized data, and 2,652 bytes of zero-initialized RAM.

## 2026-08-04 - JDY-16 BLE command channel and mobile dashboard

- Objective: preserve the USART1 wired maintenance console while adding wireless status, commands, event notifications, and CSV retrieval for an iPhone.
- CubeMX: enabled USART2 asynchronous mode on PA2/PA3 at 9600 baud and enabled its global interrupt at NVIC priority 1/0. USART1 remains PA9/PA10 at 115200 baud and priority 0/0.
- Wiring: JDY-16 TX connects to PA3/USART2_RX, JDY-16 RX connects to PA2/USART2_TX, VCC connects to 3.3 V, and both devices share GND.
- Firmware architecture: independent interrupt-fed ring buffers and command buffers are used for USART1 and USART2. Both ports execute the same command set without combining partially received input.
- App protocol: added `status_json` plus compact `@TEMP`, `@EVENT`, and `@STATUS` messages. Human-readable diagnostics remain on USART1, preventing the 9600-baud BLE channel from being flooded by one-second acceleration reports.
- Mobile UI: added `docs/mobile-app` with a Chinese dashboard for live temperature, motion state, event counts, Flash state, XYZ values, commands, confirmed log clearing, and CSV download through JDY-16 service FFE0/characteristic FFE1.
- iPhone limitation: Safari does not expose Web Bluetooth. The HTTPS-hosted page must be opened in Bluefy or another iOS Web Bluetooth bridge; a future GitHub Pages deployment will provide the required HTTPS origin.
- Browser validation: Edge/Playwright checks at 1440 px desktop and 390 px mobile completed with no page or console errors. Simulated status/event messages correctly produced 27.3 C, four motion events, three shock events, 138 records, normal Flash state, and the collision banner.
- Firmware build validation: STM32CubeIDE Debug build completed with 0 errors and 0 warnings. Firmware uses 35,392 bytes of code/constant Flash, 12 bytes of initialized data, and 2,828 bytes of zero-initialized RAM.
- Firmware download: after the ST-Link was connected, STM32CubeProgrammer identified an STM32F103 medium-density device at 3.33 V, programmed the 34.57 KB ELF image, verified it successfully, and issued a software reset.
- Hardware validation pending: verify BLE connection, `status_json`, a phone-issued command, a live motion/shock event, and CSV export before treating the feature as complete.

## 2026-08-04 - Interactive BLE record and event views

- Product refinement: replaced the always-visible command console with a quiet connection state. Periodic `status_json` polling no longer creates visible message spam; manual commands and errors remain available under the communication diagnostics panel.
- Firmware protocol: added `preview` for the most recent 100 non-boot records and `events` for the most recent 200 motion/shock records. Both commands use explicit begin/end markers so the 20-byte BLE notifications can be reconstructed safely.
- Record preview: the phone UI now parses records locally, draws a temperature trend on Canvas, shows a session/time/temperature/event table, and retains full CSV download as a separate action.
- Event interaction: motion and shock counters are independent buttons. Their side panels show motion transitions, collision acceleration magnitude, XYZ evidence, and recent event timing.
- Help behavior: help content is static in its own side panel and no longer sends `help` to firmware, so it cannot be displaced by incoming device messages.
- Deployment preparation: added a GitHub Pages workflow for an HTTPS-hosted mobile interface. iPhone still requires Bluefy because Safari does not expose Web Bluetooth.
- Browser validation: Edge/Playwright checks at 390 x 844 and 1440 x 900 completed with no console errors or horizontal page overflow. The simulated record chart contained 3,073 painted pixels and two simulated shock rows were parsed correctly.
- Firmware build validation: STM32CubeIDE Debug build completed with 0 errors and 0 warnings; firmware uses 36,476 bytes of code/constant Flash, 12 bytes of initialized data, and 2,828 bytes of zero-initialized RAM.

## 2026-08-04 - Remote pause and low-power standby controls

- Control commands: added `pause`, `resume`, and `sleep` to both USART command channels. The BLE dashboard exposes them as normal buttons rather than requiring manual command entry.
- Pause semantics: sensors, live temperature, XYZ data, OLED, BLE, and status polling continue operating, while new temperature/event records and event counters stop. Resume restarts event detection and schedules a new temperature record. Pause state is intentionally volatile and returns to recording after reset.
- Status protocol: appended a recording-paused flag to `@STATUS` without changing the existing ten fields, allowing the phone button and current-state label to recover the correct mode after reconnecting.
- Standby behavior: `sleep` acknowledges the command, turns the OLED off, turns off the active-low status LED, clears the STM32 wake flag, and enters standby. Recovery requires the board Reset button or power cycle.
- Power limitation: standby is not a physical battery disconnect. Modules powered directly from the supply, including JDY-16, can continue consuming current; a true off command requires a separate power-switch circuit.
- CubeMX impact: none. No pins, clocks, interrupts, or peripheral assignments changed.
- Browser validation: a 390 x 844 Edge/Playwright test confirmed four control buttons with no horizontal overflow or console errors. The generated BLE payloads were exactly `pause\n`, `resume\n`, `status_json\n`, and `sleep\n`.
- Firmware build validation: STM32CubeIDE Debug clean build completed with 0 errors and 0 warnings; firmware uses 37,076 bytes of code/constant Flash, 12 bytes of initialized data, and 2,828 bytes of zero-initialized RAM.
- Firmware download: STM32CubeProgrammer programmed and verified the 36.22 KB ELF through ST-Link at 3.39 V, then issued a software reset. Physical pause/resume and standby/Reset behavior remain the next hardware checks.

## 2026-08-04 - BLE-preserving UART wake revision

- Design revision: replaced the previous reset-only STM32 Standby command with ordinary Cortex-M Sleep so the existing JDY-16 BLE connection and USART2 receiver remain active.
- Firmware sequence: `sleep` acknowledges with `@SLEEPING`, turns off the OLED and active-low status LED, suspends SysTick, and enters `HAL_PWR_EnterSLEEPMode`. The first USART2 receive interrupt returns the CPU from `WFI`; firmware then resumes SysTick, restores the LED/OLED, and emits `@AWAKE`.
- Phone behavior: after `@SLEEPING`, the dashboard stops its three-second status poll, keeps the BLE characteristic connected, changes the button to `唤醒设备`, and rejects unrelated commands. Pressing the wake button sends `wake`; after `@AWAKE`, polling and the normal online state resume.
- CubeMX impact: none. USART2 RX interrupts were already enabled on PA3, so no pins, NVIC settings, clocks, or generated initialization changed.
- Timing limitation: SysTick is intentionally suspended in Sleep, so without an RTC the sleeping duration is not included in record `time_s`. Sensor and event sampling also pause until wake.
- Power limitation: ordinary Sleep consumes more than the former Standby implementation, and JDY-16 remains powered. This change prioritizes phone-controlled recovery over maximum battery life.
- Browser validation: Edge/Playwright at 390 x 844 confirmed that polling stops while sleeping, the BLE characteristic remains available, unrelated refresh is blocked, and the emitted sequence is exactly `sleep\n`, `wake\n`, then `status_json\n`. No console errors or horizontal overflow occurred.
- Firmware build validation: STM32CubeIDE Debug clean build completed with 0 errors and 0 warnings; firmware uses 37,284 bytes of code/constant Flash, 12 bytes of initialized data, and 2,828 bytes of zero-initialized RAM.
- Firmware download: STM32CubeProgrammer programmed and verified the 36.42 KB ELF through ST-Link at 3.56 V, then issued a software reset. A real phone sleep/wake cycle remains the final hardware check.

## 2026-08-04 - Reset recovery while the dashboard is sleeping

- Observed failure: pressing the hardware Reset button while the dashboard showed `设备休眠` restarted the STM32, but the browser retained `deviceSleeping=true`, kept polling stopped, and continued rejecting normal commands.
- Browser fix: centralized awake-state recovery. While marked sleeping, receipt of `@AWAKE`, `@TEMP`, `@STATUS`, or `@EVENT` now restores the online label, sleep button, and periodic polling. A temperature/event wake also requests a complete status refresh; an existing status frame is used directly without duplicating the request.
- Firmware reinforcement: after USART2 interrupt reception initializes successfully on every boot, the STM32 now sends `@AWAKE` through the BLE UART. Reset recovery therefore does not need to wait for the first DS18B20 conversion.
- Browser validation: simulated Reset recovery through both `@TEMP` and `@STATUS` cleared the sleeping flag, restored the `设备休眠` command label and `设备在线` state, restarted polling, and produced no console errors or horizontal overflow.
- CubeMX impact: none. USART2 PA2/PA3 and its existing receive interrupt configuration are unchanged.
- Firmware build validation: STM32CubeIDE Debug clean build completed with 0 errors and 0 warnings; firmware uses 37,340 bytes of code/constant Flash, 12 bytes of initialized data, and 2,828 bytes of zero-initialized RAM.
- Firmware download: STM32CubeProgrammer programmed and verified the 36.48 KB ELF through ST-Link at 3.41 V, then issued a software reset. The remaining check is a real phone sequence of sleep, hardware Reset, and automatic dashboard recovery.

## 2026-08-04 - Chronological mobile log views

- Display order: record preview, motion details, and shock details now retain the firmware transfer order, showing the earliest record at the top and later records below it.
- Browser validation: an Edge/Playwright mobile test injected ordered record and event samples. The rendered record times were 10/20/30 seconds, motion times were 12/18/22 seconds, and shock times were 15/25 seconds, with no console errors or horizontal overflow.
- RTC follow-up: after the real-time clock module is integrated, replace session-relative `time_s` labels with real calendar timestamps in exported records, mobile tables, and the temperature-chart x-axis.
- CubeMX and firmware impact: none. This change affects only the mobile web interface and documentation, so no code generation, firmware build, or board reflash is required.

## 2026-08-04 - BLE runtime parameter configuration

- Product goal: allow motion and shock detection to be calibrated after enclosure assembly without reopening the device or reconnecting ST-Link for every threshold adjustment.
- Runtime parameters: shock magnitude threshold, motion deviation from 1 g, motion confirmation time, still confirmation time, shock cooldown, and periodic temperature-log interval are now adjustable through compact `cfg` UART/BLE commands.
- Detection behavior: the default values preserve the previous behavior: 1600 mg shock threshold, 200 mg motion deviation, 60 ms motion confirmation, 1000 ms still confirmation, 500 ms shock cooldown, and 60-second temperature logging.
- Persistence: `cfg save` serializes the six values with a format marker and CRC, erases only the dedicated W25Q64 sector at `0x7FE000`, writes 32 bytes, reads them back, and compares the result. Invalid or missing configuration falls back to defaults.
- Storage isolation: the configuration sector is separate from the 64 KB circular log at `0x000000`-`0x00FFFF` and the persistence-test sector at `0x7FF000`.
- Mobile UI: added a Chinese parameter panel with numeric range enforcement, units, short behavior explanations, read/save/default actions, and explicit permanent-save feedback.
- CubeMX impact: none. Existing SPI2 W25Q64 storage and USART2 JDY-16 communication are reused; no pins, clocks, interrupts, or generated initialization changed.
- Browser validation: Edge/Playwright at 390 x 844 verified six-field configuration fill, the exact seven-command save sequence, the 20-byte BLE packet limit, persistent-save feedback, no console errors, and no horizontal overflow.
- Firmware build validation: STM32CubeIDE Debug clean build completed with 0 errors and 0 warnings. Firmware uses 39,892 bytes of code/constant Flash, 12 bytes of initialized data, and 2,860 bytes of zero-initialized RAM.
- Firmware download: STM32CubeProgrammer detected the STM32F103 medium-density device at 3.47 V, programmed the 38.97 KB ELF, verified it successfully, and issued a software reset.
- Hardware validation: the user changed the shock threshold from 1600 mg to 1800 mg through the BLE parameter panel, saved it, power-cycled the device, reconnected, and read back 1800 mg. This confirms the complete phone-command, firmware-application, W25Q64 persistence, and startup-recovery path on real hardware.
- Static baseline: after restoring the 1600 mg default shock threshold, the assembled breadboard device remained stationary on a desk for two minutes without false motion or shock events, while live sensor data continued updating normally.

## 2026-08-05 - DS1302 RTC bring-up prepared

- Hardware identification: MH-Real-Time Clock Modules-2 carrying a DS1302, a 32.768 kHz crystal, and a CR2032 backup cell. The five interface pins are VCC, GND, CLK, DAT, and RST.
- CubeMX pin assignment: added PA4 `RTC_CLK`, PA5 `RTC_DAT`, and PA6 `RTC_RST` as low-speed GPIO outputs. Existing USART, I2C, SPI, DS18B20, and status-LED assignments are unchanged.
- Wiring plan: DS1302 VCC to 3.3 V, GND to common GND, CLK to PA4, DAT to PA5, and RST to PA6.
- Firmware: added a three-wire, least-significant-bit-first DS1302 driver with bidirectional DAT control, BCD conversion, date validation, weekday calculation, write protection, and the trickle charger explicitly disabled when setting time because the module uses a non-rechargeable CR2032.
- Commands: `time` reads the clock; `time set YYYY-MM-DD HH:MM:SS` validates and writes a new value, then reads it back. The command buffer increased from 24 to 40 bytes to hold the full timestamp command.
- Scope boundary: real timestamps are not yet written to W25Q64 records or shown on OLED/mobile views. First verify reading, ticking, reset retention, and main-power-off retention on hardware.
- Build validation: STM32CubeIDE Debug clean build completed with 0 errors and 0 warnings. Firmware uses 43,072 bytes of code/constant Flash, 12 bytes of initialized data, and 2,908 bytes of zero-initialized RAM.
- Hardware validation pending: flash the firmware, set the time, confirm seconds advance, disconnect main power for at least one minute while leaving the CR2032 installed, then reconnect and confirm the elapsed time was retained.
- Initial hardware finding: the first DS1302 read implementation intermittently returned a valid-looking timestamp whose BCD bytes were shifted right by one bit, for example year `0x26` becoming `0x13`. The cause was DAT bus contention because the STM32 changed the bidirectional data pin to input after, rather than before, the final command-clock falling edge.
- Timing fix: the read-command path now releases DAT while CLK is still high before the final falling edge, allowing the DS1302 to drive the first response bit without contention.
- Firmware download: STM32CubeProgrammer detected the STM32F103 at 3.37 V, programmed the 42.21 KB ELF, verified it successfully, and issued a software reset.
- Hardware validation: COM4 was opened at 115200 baud and the RTC was set to `2026-08-05 11:42:07`. Three later reads returned `11:42:10`, `11:42:12`, and `11:42:14` without shifted values, confirming stable write, readback, and second counting. Main-power-off retention remains to be tested.
- Separate observation: DS18B20 reported not connected during this RTC test; ADXL345 continued returning acceleration data. This does not affect the RTC result.

## 2026-08-05 - End-to-end RTC timestamps for Flash and BLE

- Storage format: reused bytes 24-29 of the existing fixed 32-byte W25Q64 record for year offset, month, day, hour, minute, and second. Record capacity remains 2048 and the existing CRC now protects the timestamp as part of the same 30-byte payload.
- Backward compatibility: records written before this firmware contain zeros in the former reserved bytes. They remain readable and are exported as `NA`; no Flash erase or data migration is required.
- Firmware sampling: the DS1302 is refreshed once per second. Temperature records, motion transitions, shock events, and boot delimiters receive the latest valid RTC timestamp; after Sleep wake-up the RTC cache is refreshed immediately.
- BLE protocol: appended a timestamp to `@TEMP`, `@EVENT`, and `@STATUS` while preserving all existing field positions. The mobile interface therefore remains compatible with older messages and shows the live device clock when the new field is present.
- Export format: simplified CSV now uses `session,timestamp,time_s,temp_c,motion,shock`. Raw, preview, and event streams also include `timestamp`; old records continue to expose their session-relative `time_s`.
- Mobile interface: added the current DS1302 time to the online status area, full timestamps to record/motion/shock tables, and a temperature chart whose horizontal positions reflect real elapsed time instead of equal record spacing. Legacy records fall back to chronological session-relative display.
- CubeMX impact: none in this step. The previously configured PA4 `RTC_CLK`, PA5 `RTC_DAT`, and PA6 `RTC_RST` assignments are reused.
- Firmware build validation: STM32CubeIDE Debug clean build completed with 0 errors and 0 warnings. Firmware uses 44,024 bytes of code/constant Flash, 12 bytes of initialized data, and 2,924 bytes of zero-initialized RAM.
- Browser validation: Edge/Playwright passed at 390 x 844 and 1440 x 900 with no console errors or horizontal overflow. Simulated new-format status, preview, motion, and shock frames displayed full timestamps; an old-format preview remained readable; the chart rendered nonblank with a real-time x-axis.
- Hardware download pending: STM32CubeProgrammer currently reports `ST-LINK error (DEV_CONNECT_ERR)`, so this integrated image has not yet been flashed. Reconnect board power and the SWD wiring, then repeat programming and verify `status_json` plus a new raw log record.

## 2026-08-05 - Permanent mobile dashboard deployment

- Deployment change: replaced the temporary `trycloudflare.com` tunnel as the normal phone entry point with GitHub Pages at `https://hansolo2r.github.io/stm32-transport-logger/`.
- Repository: published the reviewed CubeMX/CubeIDE project, firmware sources, mobile dashboard, screenshots, and engineering log to the public `Hansolo2r/stm32-transport-logger` repository.
- Repository hygiene: excluded generated Debug/Release artifacts, IDE-local settings, launch files, and the obsolete top-level `Inc/Src` tree. Commit authorship uses the GitHub noreply address.
- Automation: `.github/workflows/pages.yml` uploads only `docs/mobile-app` on each push to `main`, so future dashboard changes automatically replace the fixed HTTPS site without requiring the development PC to stay online.
- Deployment validation: the first workflow run occurred before Pages enablement and failed at `configure-pages`. After enabling workflow-based Pages and rerunning, all checkout, configuration, upload, and deployment steps passed. A separate public request returned HTTP 200 with `text/html; charset=utf-8`.
- iPhone usage: open the fixed HTTPS address inside Bluefy because iOS Safari does not expose Web Bluetooth. The GitHub Pages origin is stable; it does not depend on the local Python server or Cloudflare tunnel process.

## 2026-08-05 - Mixed-generation RTC chart axis fix

- Observed issue: the temperature chart enabled its real-time x-axis only when every previewed temperature record had an RTC timestamp. Existing Flash commonly contains pre-RTC records, so mixing old and new records incorrectly hid the real-time labels.
- Parsing behavior: the chart now selects all valid timestamped temperature records when at least two are available. Old compatible records remain visible in the table but no longer force new RTC chart data back to record-order mode.
- Axis presentation: added a visible horizontal baseline, increased the chart height from 210 to 230 pixels, reserved bottom label space, and displays approximate labels as `MM-DD HH:MM` without seconds.
- Browser validation: a mixed preview containing two `NA` legacy timestamps and two valid RTC timestamps rendered `08-05 12:05` and `08-05 12:18` at both 390 x 844 and 1440 x 900. Both sizes had zero console errors, zero horizontal overflow, and nonblank canvas pixels.
- Firmware and CubeMX impact: none. This correction affects only the mobile dashboard and requires no board reflash.

## 2026-08-05 - Approximate RTC anchoring for legacy temperature points

- Field observation: the phone preview contained new RTC-stamped motion/shock records whose temperature was unavailable, while valid historical temperature records had only session-relative `time_s`. Neither subset alone provided two points with both temperature and calendar time.
- Approximation method: for a temperature point without RTC data, the dashboard selects the nearest timestamped record in the same session and applies the difference between their `time_s` values. This yields an approximate calendar time without modifying stored Flash records.
- Evidence boundary: estimated labels are explicitly marked `推算时间轴`; directly timestamped temperature points remain marked `真实时间轴`. When no same-session RTC anchor exists, the chart shows `会话时间轴`, and multi-session unanchored data falls back to older/newer record order.
- Browser validation: a 390 x 844 test reproduced the reported structure with two RTC-stamped events containing `NA` temperature and three legacy temperature points. The chart displayed `08-05 12:18` to `08-05 12:20`, reported `推算时间轴`, produced no console errors, and had zero horizontal overflow.
- Firmware and CubeMX impact: none. The correction is isolated to the mobile dashboard and requires only a page refresh after deployment.

## 2026-08-05 - Breadboard integration diagram

- Deliverable: added `docs/wiring/breadboard-wiring.svg` and a rendered PNG showing the recommended 830-point breadboard placement and the exact active firmware pin mapping.
- Placement guidance: the STM32 board spans the center trench; W25Q64 is kept near SPI2; ADXL345 is mounted flat and rigid; the OLED faces the enclosure window; DS18B20 remains exposed; DS1302 stays battery-accessible; and the JDY-16 antenna is kept clear of metal and the battery.
- Power guidance: marks both 3.3 V and common-GND rails, the center split that must be bridged, and the rule that SDO cannot substitute for the ADXL345 ground connection.
- Signal coverage: documents DS18B20 PA8, DS1302 PA4/PA5/PA6, JDY-16 PA2/PA3, OLED PB8/PB9, ADXL345 PB10/PB11 plus CS/SDO mode straps, and W25Q64 PB12-PB15.
- Evidence boundary: physical positions are a recommended layout because module PCB dimensions and header direction can vary; the signal-to-pin mapping is the source-of-truth connection list for the current CubeMX configuration.
- Validation: rendered the SVG through headless Edge at 2400 x 1800 and visually checked text contrast, clipping, component overlap, rail bridging, and pin labels. No firmware or CubeMX changes were made.

## 2026-08-05 - Final enclosure breadboard plan

- Objective: replace the initial generic breadboard drawing with an assembly reference based on the real 830-hole board, the photographed module proportions, and the purchased power-component dimensions.
- Selected layout: front-mounted battery with a left power zone, central STM32/storage/motion zone, and right display/wireless/cold zone. The ADXL345 sits near the assembled device center; the JDY-16 antenna and DS18B20 sensor face outward.
- Exact purchased dimensions represented: 40 x 20 x 6 mm LiPo battery, 18 x 12 x 4.2 mm charger, 17 x 13 x 4.2 mm TPS63020, and 12.7 x 6.7 mm SS12D10 switch body.
- Deliverables: `final-breadboard-placement`, `final-power-wiring`, `final-signal-wiring`, and `final-assembly-sequence`, each provided as editable SVG and rendered PNG under `docs/wiring`.
- Power design: documents the battery-charger-switch-TPS63020-3.3 V chain, six required breadboard rail bridges, center-plus-one-outer-pin switch use, multimeter verification, and the prohibition on simultaneous battery, USB-C, ST-Link 3.3 V, or USB-UART VCC power.
- Signal design: rechecked against `project 1.ioc`; no firmware or CubeMX changes were required. The diagrams retain PA8, PB8/PB9, PB10/PB11, PB12-PB15, PA4-PA6, and PA2/PA3 assignments.
- Safety boundary: the charger pad order remains conditional until the received PCB silkscreen is visible. The diagram intentionally does not guess B/OUT pad locations from a product photograph.
- Validation: rendered all four SVG files with headless Edge at their native 1800-pixel width and visually checked Chinese text contrast, clipping, module overlap, signal direction, power polarity, and staged test criteria.
- Drawing correction after physical-board review: replaced the generic continuous dot grid with the real 830-hole structure: top and bottom `+/-` power-rail rows, five `A-E` terminal rows, the center trench, and five `F-J` terminal rows. The power diagram now labels all four physical rail rows explicitly; module placement and firmware pin assignments are unchanged.
