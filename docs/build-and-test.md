# Build and test

## Firmware build

1. Open this directory as an existing STM32CubeIDE project.
2. Use `project 1.ioc` when changing pins, clocks, or peripherals.
3. Select the `Debug` configuration and build `project 1`.

The current target is an STM32F103C8T6 with the HAL and CMSIS sources included
in the repository. Flashing requires an ST-Link and a powered board. A
successful build does not prove that the wiring or sensors work.

## Event-detector tests

With a C11 compiler available:

```powershell
$output = Join-Path $env:TEMP "event_detector_test.exe"
gcc -std=c11 -Wall -Wextra -Werror `
  -ICore/Inc `
  tests/firmware/event_detector_test.c `
  Core/Src/event_detector.c `
  -o $output
& $output
```

Expected result:

```text
PASS event detector: 15 scenarios
```

## Dashboard tests

The browser tests use Node.js, Playwright, and Microsoft Edge on Windows.

```powershell
npm install --no-save --package-lock=false playwright
node --test docs/mobile-app/tests/command-utils.test.cjs docs/mobile-app/tests/time-sync-browser.test.cjs
node --check docs/mobile-app/app.js
```

The dashboard itself has no build step. GitHub Pages deploys
`docs/mobile-app` after a push to `main`.
