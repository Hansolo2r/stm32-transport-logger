# Motion and Shock Classification Design

## Goal

Make event labels match normal human interpretation:

- A shock is a short, high-amplitude acceleration pulse lasting no more than 200 ms.
- Motion is sustained activity confirmed over 1000 ms.
- When activity starts from rest, the same initial movement must not be counted as both motion and shock.
- A separate short impact that occurs while the device is already moving must still be recorded as a shock.

## Sampling and Inputs

The ADXL345 is sampled every 10 ms (100 Hz). Detection uses the magnitude of the three-axis acceleration vector and compares it with:

- the configurable shock threshold;
- the configurable motion deviation around 1 g.

No CubeMX pin, clock, I2C, SPI, UART, or GPIO configuration changes are required.

## State Model

### Resting

The detector starts in the resting state. Samples outside the motion dead band add to a motion evidence counter; samples inside the dead band reduce that counter. This tolerates brief quiet samples during real movement while preventing a single impact from becoming a motion event.

A sample above the shock threshold starts a shock pulse candidate. Consecutive high samples measure the pulse duration.

### Classifying an event that starts from rest

- If motion evidence reaches 1000 ms, emit `MOTION_START`, enter the moving state, and discard any shock candidate caused by the same startup action.
- If the motion evidence returns to zero before motion is confirmed, and the completed shock pulse lasted from 10 ms through 200 ms, emit one `SHOCK` event.
- A high level lasting longer than 200 ms is not a shock. It can contribute only to motion detection.

This deliberately delays classification of an event that begins from rest until enough temporal evidence exists.

### Moving

While moving, a completed high-amplitude pulse lasting no more than 200 ms emits a `SHOCK` event independently. The existing shock cooldown prevents repeated counts from one vibration burst.

When acceleration remains inside the still band for the configurable still-confirmation time, emit `MOTION_END` and return to resting.

## Configuration

Default values after the firmware update:

- shock threshold: 2200 mg;
- maximum shock duration: 200 ms;
- motion deviation: 150 mg;
- motion confirmation: 1000 ms;
- still confirmation: 1500 ms;
- shock cooldown: 800 ms.

The maximum shock duration is added as a saved configuration field and exposed through BLE and the mobile configuration panel. Existing saved configuration uses format marker `TCF1`; the new layout uses a new marker so incompatible old data is rejected and safe defaults are loaded once. The user can then save the new parameters to W25Q64.

## Tests

Host-side detector tests will cover:

1. A 50 ms high pulse followed by rest produces one shock and no motion.
2. Activity sustained for 1000 ms produces one motion start and suppresses its startup shock candidate.
3. A high level longer than 200 ms does not produce a shock.
4. A short high pulse during active motion produces an additional shock.
5. Brief activity shorter than 1000 ms and below the shock threshold produces neither event.
6. Sustained stillness ends motion only after the configured still-confirmation time.

After host tests pass, the complete STM32 project must build with zero errors. Compilation does not prove physical behavior; final thresholds require a hardware test after flashing.

## User-visible Changes

- The mobile parameter panel gains a `maximum shock duration` field in milliseconds.
- Parameter read/save messages include the new value.
- Existing logs and CSV event names remain compatible: `SHOCK`, `MOTION_START`, and `MOTION_END`.
