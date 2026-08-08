# Motion and Shock Classification Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace independent instantaneous thresholds with a tested dual-timescale detector that distinguishes a short shock of at most 200 ms from motion confirmed over 1000 ms.

**Architecture:** Put the portable event state machine in `event_detector.c` so the same C implementation runs in host tests and STM32 firmware. `app.c` remains responsible for ADXL345 sampling, logs, OLED/BLE notifications, cooldown, and saved configuration. The mobile page reads and writes the new maximum-shock-duration parameter.

**Tech Stack:** C11 host tests with MinGW GCC, STM32 HAL/CubeIDE, vanilla JavaScript/Node tests, W25Q64 configuration storage.

## Global Constraints

- ADXL345 sampling remains 10 ms (100 Hz).
- Maximum shock duration defaults to 200 ms.
- Motion confirmation defaults to 1000 ms.
- A startup shock candidate is suppressed when the same action becomes sustained motion.
- A short impact that starts while already moving is still recorded.
- CubeMX pin, clock, and peripheral configuration remains unchanged.
- Compilation, flashing, and physical validation are reported separately.

---

### Task 1: Portable Event Detector

**Files:**
- Create: `Core/Inc/event_detector.h`
- Create: `Core/Src/event_detector.c`
- Create: `tests/firmware/event_detector_test.c`

**Interfaces:**
- Consumes: 10 ms acceleration samples in mg and `EventDetectorConfig` thresholds.
- Produces: `EventDetector_Update(EventDetector *, int16_t, int16_t, int16_t)` returning `EVENT_DETECTOR_SHOCK`, `EVENT_DETECTOR_MOTION_START`, or `EVENT_DETECTOR_MOTION_END` bits.

- [ ] **Step 1: Write host tests for the six specified event sequences**

Create table-driven helpers that feed 10 ms samples. Assert that a 50 ms 2600 mg pulse produces only shock; 1000 ms of 1400 mg activity produces motion start and no startup shock; 300 ms above the shock threshold produces no shock; a 50 ms pulse while moving produces shock; 500 ms of sub-shock activity produces no event; and 1500 ms of still samples produces motion end.

- [ ] **Step 2: Compile the tests and verify RED**

Run:

```powershell
D:\mingw64\bin\gcc.exe -std=c11 -Wall -Wextra -Werror -ICore\Inc tests\firmware\event_detector_test.c Core\Src\event_detector.c -o tests\firmware\event_detector_test.exe
```

Expected: failure because `event_detector.h` and the detector functions do not exist.

- [ ] **Step 3: Implement the detector state machine**

Define a state containing motion-active state, leaky motion evidence, consecutive still samples, current shock-pulse length, pending startup shock, and suppression state. Convert configured milliseconds to sample counts with ceiling division by the configured 10 ms interval. Saturate counters to avoid overflow.

- [ ] **Step 4: Compile and run the detector tests**

Run the GCC command above, then:

```powershell
tests\firmware\event_detector_test.exe
```

Expected: all six scenarios pass with exit code 0.

### Task 2: Firmware Integration and Saved Configuration

**Files:**
- Modify: `Core/Src/app.c`
- Modify: `Core/Inc/event_detector.h`
- Test: `tests/firmware/event_detector_test.c`

**Interfaces:**
- Consumes: event bits from Task 1 and a new `shock_max_duration_ms` configuration field.
- Produces: unchanged log/BLE event names and an expanded `@CFG` record containing seven numeric values.

- [ ] **Step 1: Add failing configuration assertions to the host test**

Assert that the default detector configuration uses 2200 mg, 150 mg, 1000 ms, 1500 ms, and 200 ms. Verify boundary behavior at exactly 200 ms and at 210 ms.

- [ ] **Step 2: Verify the new assertions fail**

Compile and run the host test. Expected: failure until the production defaults and exact boundary behavior exist.

- [ ] **Step 3: Integrate the detector in `app.c`**

Replace `motion_candidate_samples` and direct shock/motion threshold branches in `HandleAccelerationEvent` with `EventDetector_Update`. Keep the existing logging, OLED alert, BLE event, counts, and cooldown actions. Mirror detector motion state into status output.

- [ ] **Step 4: Extend configuration storage and commands**

Add `shock_max_duration_ms` with valid range 10-1000 ms, default 200 ms, command `cfg shockms <value>`, and append it to `@CFG`. Store it at bytes 16-17, change the marker from `TCF1` to `TCF2`, and retain the existing CRC. Old saved configuration must fall back to the new defaults rather than being decoded incorrectly.

- [ ] **Step 5: Run host tests again**

Expected: all detector and boundary tests pass with `-Wall -Wextra -Werror`.

- [ ] **Step 6: Build the complete STM32 project**

Run:

```powershell
E:\stm32\STM32CubeIDE_2.2.0\STM32CubeIDE\stm32cubeidec.exe -nosplash -application org.eclipse.cdt.managedbuilder.core.headlessbuild -data E:\stm32\.codex_build_workspace -build "project 1/Debug"
```

Expected: `Build Finished. 0 errors, 0 warnings.` This verifies compilation only.

### Task 3: Mobile Parameter Control

**Files:**
- Modify: `docs/mobile-app/index.html`
- Modify: `docs/mobile-app/app.js`
- Modify: `docs/mobile-app/tests/time-sync-browser.test.cjs`

**Interfaces:**
- Consumes: seven-field `@CFG` records.
- Produces: `cfg shockms <value>` before `cfg save` and a visible maximum-shock-duration input.

- [ ] **Step 1: Add a failing browser test**

Extend the BLE stub test to inject `@CFG,2200,150,1000,1500,800,60,200`, assert that the UI displays 200 ms, submit 250 ms, and assert that reconstructed BLE commands include `cfg shockms 250` followed by `cfg save`.

- [ ] **Step 2: Run browser and utility tests to verify RED**

Run:

```powershell
node --test docs/mobile-app/tests/*.test.cjs
```

Expected: the new assertion fails because the field is absent.

- [ ] **Step 3: Add the mobile field and protocol handling**

Add a numeric input labelled `碰撞最长持续时间`, range 10-1000 ms, parse the seventh `@CFG` value, and include `cfg shockms` in the save sequence. Keep all displayed text in Chinese.

- [ ] **Step 4: Run all web tests**

Expected: all Node and Playwright tests pass, with no console errors or mobile horizontal overflow.

### Task 4: Documentation and Final Verification

**Files:**
- Modify: `docs/development-log.md`
- Modify: `docs/superpowers/plans/2026-08-08-motion-shock-classification.md`

**Interfaces:**
- Consumes: test and build output from Tasks 1-3.
- Produces: factual GitHub-ready evidence that separates host tests, compilation, flashing, and physical validation.

- [ ] **Step 1: Record the implementation and evidence**

Document the old false-positive mechanism, the new temporal state model, defaults, configuration migration, host-test results, and CubeIDE build result. Mark flashing and physical classification tests as pending until the user performs them.

- [ ] **Step 2: Run final verification**

Run host tests, web tests, `node --check docs/mobile-app/app.js`, the CubeIDE headless build, and `git diff --check`. Confirm no unrelated files are modified.

- [ ] **Step 3: Explain the exact changes and flashing procedure**

Report that CubeMX has no changes; identify `event_detector.c`, `app.c`, and the mobile parameter panel; explain `mg`, sample counts, leaky evidence, and state transitions in beginner-friendly Chinese. Give the expected physical tests for a tap, pickup-and-carry, sustained shake, and impact while moving.
