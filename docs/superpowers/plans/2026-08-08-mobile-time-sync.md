# Mobile Time Synchronization Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Let the existing mobile dashboard set the DS1302 from the phone clock even though JDY-16 BLE writes are limited to 20-byte packets.

**Architecture:** Add a small browser utility that formats the local phone time and splits UTF-8 commands into 20-byte chunks without adding intermediate newlines. The existing command sender will write chunks sequentially, and a new dashboard button will send the generated `time set` command before requesting status.

**Tech Stack:** Plain JavaScript, Web Bluetooth, Node.js built-in test runner, existing HTML/CSS dashboard.

## Global Constraints

- Do not change STM32 firmware or `project 1.ioc`.
- Preserve all existing short-command behavior.
- Each BLE write must contain at most 20 bytes.
- Append exactly one newline after the complete command, not after each chunk.
- Display Chinese user-facing status messages.

---

### Task 1: Add tested BLE command utilities

**Files:**
- Create: `docs/mobile-app/command-utils.js`
- Create: `docs/mobile-app/tests/command-utils.test.cjs`

**Interfaces:**
- Produces: `window.CommandUtils.formatTimeSetCommand(date)` and `window.CommandUtils.splitCommandPayload(command, maxBytes)`.

- [x] Write tests for exact local-time formatting, 20-byte chunk limits, byte-preserving reconstruction, and a single final newline.
- [x] Run the Node test and verify it fails because `command-utils.js` does not exist.
- [x] Implement the minimal utility functions.
- [x] Run the Node test and verify all assertions pass.

### Task 2: Integrate phone-time synchronization

**Files:**
- Modify: `docs/mobile-app/index.html`
- Modify: `docs/mobile-app/app.js`
- Modify: `docs/mobile-app/styles.css` only if the existing button styles are insufficient.

**Interfaces:**
- Consumes: `window.CommandUtils` from Task 1.
- Produces: `#syncTimeButton` and sequential chunked writes through the existing BLE characteristic.

- [x] Load `command-utils.js` before `app.js` and add a visible `同步手机时间` command button.
- [x] Change `sendCommand()` to write all generated chunks sequentially instead of rejecting payloads over 20 bytes.
- [x] On button press, generate the phone-local timestamp, send it, show a concise result, and request `time`/`status_json` after the write completes.
- [x] Run utility tests and JavaScript syntax checks.
- [x] Exercise the dashboard with a stub BLE characteristic and verify the time command reconstructs correctly from multiple writes.
- [x] Render mobile and desktop screenshots and verify no overlap, horizontal overflow, or console errors.

### Task 3: Document and deploy

**Files:**
- Modify: `docs/development-log.md`

- [x] Record the reason for BLE packet splitting, the new time-sync workflow, and test evidence.
- [x] Review the diff and run whitespace checks.
- [ ] Commit and push to `main` so GitHub Pages deploys the updated dashboard.
