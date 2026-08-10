# Evidence Package

This directory contains source material for a future GitHub presentation. It
keeps raw evidence separate from README wording and from generated diagrams.

## Archived Images

### Battery-powered breadboard prototype

![Battery-powered breadboard prototype](images/2026-08-08-breadboard-battery-prototype.jpg)

- File: `images/2026-08-08-breadboard-battery-prototype.jpg`
- Original dimensions: 1280 x 1707
- Original size: 361,383 bytes
- SHA-256: `10AB409336913072C3324DCEBDF438AF15CBC70FC8088C3485C7B9A6203CF10C`
- What it supports: the modules were physically assembled on a breadboard and
  powered from the LiPo/charger/regulator chain; the OLED and board indicators
  were active when photographed.
- Packaging decision: on 2026-08-10 the user selected this breadboard-mounted
  arrangement as the basis of the final portable prototype rather than moving
  the circuit to a custom PCB.
- What it does not support: enclosure completion, battery-life duration,
  calibration accuracy, or operation during a real shipment.

### Live iPhone dashboard

![Live iPhone dashboard](images/2026-08-08-mobile-dashboard-live.jpg)

- File: `images/2026-08-08-mobile-dashboard-live.jpg`
- Original dimensions: 1179 x 2556
- Original size: 169,918 bytes
- SHA-256: `333E74EA1642E015E5462FFA26EEF0401176DEB0B937EAF070662C6EBED896FA`
- Visible state: JDY-16 online, DS1302 timestamp `2026-08-08 17:15:21`,
  temperature `25.0 C`, static state, 7 motion events, 26 shock events,
  116 log records, normal storage, and live XYZ values.
- What it supports: the deployed phone interface received and displayed a live
  device status packet through BLE.
- What it does not support: accuracy of every stored record or long-term radio
  reliability.

The images are retained without editing so they remain traceable to the files
provided by the user. A future public README may use cropped copies, but should
keep these originals unchanged.

Metadata check: neither archived JPEG contains GPS EXIF properties. The files
retain a small number of non-location image properties; preserve the originals
here and strip metadata only from separately generated public copies if needed.

## Other Evidence Locations

| Evidence | Location | Meaning |
| --- | --- | --- |
| Firmware source | `Core/Src`, `Core/Inc` | Current implementation |
| Hardware configuration | `project 1.ioc` | CubeMX pin and peripheral source of truth |
| Detector tests | `tests/firmware/event_detector_test.c` | Portable synthetic behavior tests |
| Dashboard tests | `docs/mobile-app/tests/` | Browser protocol and layout tests |
| Engineering history | `docs/development-log.md` | Failures, fixes, build and physical observations |
| Wiring references | `docs/wiring/` | Assembly guidance, not measurement evidence |
| Hardware BOM | `docs/evidence/bom.md` | Confirmed device modules and development-only tools |
| Real simplified CSV | `docs/evidence/data/2026-08-10-serial-transport-log.csv` | Unmodified USART1 `export` rows from W25Q64 |
| Real engineering CSV | `docs/evidence/data/2026-08-10-serial-transport-log-raw.csv` | Unmodified USART1 `export_raw` rows with record types and XYZ values |
| Serial capture manifest | `docs/evidence/data/2026-08-10-serial-export.txt` | Port settings, validation counts, and file hashes |
| Manual/automatic consistency check | `docs/evidence/data/2026-08-10-data-consistency.md` | Confirms 378 shared simplified rows match exactly |
| Device and parameter snapshot | `docs/evidence/data/2026-08-10-device-state.txt` | Read-only status, RTC, Flash state, XYZ, and detector settings |

## Evidence Rules

- Preserve raw images and exported CSV files unchanged.
- Record test conditions before interpreting results.
- Do not convert simulated browser messages into hardware evidence.
- Do not present a successful build as proof of sensor or wiring behavior.
- Do not describe the breadboard prototype as an enclosed or certified product.
- Treat the 2026-08-10 CSV files as unlabeled development data, not an event
  accuracy benchmark or a real-shipment trial.
- Physical event checks used hand-held movement and light impacts. Detector
  thresholds and timing are configurable, but other setups require their own
  calibration and validation.
