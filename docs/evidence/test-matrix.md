# Validation Matrix

Status meanings:

- `PASS`: directly observed on hardware or produced by an automated test.
- `PARTIAL`: some required behavior was observed, but the full claim is open.
- `MISSING`: no archived evidence is currently available.

| Area | Status | Evidence | Boundary or next check |
| --- | --- | --- | --- |
| STM32 compilation | PASS | Latest CubeIDE Debug build: 0 errors, 0 warnings | Compilation only |
| Firmware download | PASS | CubeProgrammer verified 44.45 KB ELF at 3.41 V | Does not prove every runtime path |
| Detector unit tests | PASS | `tests/firmware/event_detector_test.c`, 15 scenarios | Synthetic acceleration sequences |
| DS18B20 live temperature | PASS | OLED and live dashboard observations | No reference-thermometer calibration dataset |
| SSD1306 display | PASS | Prototype photograph and development log | Long-term display reliability not measured |
| ADXL345 XYZ acquisition | PASS | Live dashboard XYZ and hardware tests | No calibrated shaker/reference accelerometer |
| Motion start/end | PASS | Hand-held checks confirmed final three-axis motion and return to static | Other mounting and motion ranges require calibration |
| Shock versus motion separation | PASS | Light-impact checks covered four physical behaviors in the development log | No controlled impact-energy labels or vehicle-impact test |
| W25Q64 JEDEC/read/write | PASS | EF 40 17, program/readback and retention log | Wear lifetime not measured |
| Circular log continuation | PASS | Continued beyond former 128-record boundary | Repository has no raw 2,048-record export |
| BLE status and commands | PASS | Live iPhone screenshot and parameter persistence test | Range and disconnect recovery not quantified |
| Persistent configuration | PASS | Power-cycle test plus `cfg get` snapshot of final 2200/150 mg thresholds | Current final parameters need enclosure review |
| DS1302 read/write/ticking | PASS | Stable timestamp reads and live dashboard clock | Main-power-off retention interval is not closed |
| Pause/resume and sleep/wake | PARTIAL | Firmware/browser paths and prior interactive checks | No formal battery-current measurement |
| Battery-powered prototype | PASS | Archived breadboard photograph | Runtime duration unknown |
| CSV export protocol | PASS | Manual and automated USART1 exports match across 378 shared rows; raw and simplified files archived | Bluefy iOS Blob download remains unsupported; current dataset is unlabeled |
| Breadboard prototype packaging | PARTIAL | Archived powered prototype photo; user reports LiPo taped to the rear | Rear battery fixation and any charger/regulator or cable restraint still need a final photograph |
| Real transport trial | MISSING | None | Define route, duration, labels, and references first |
| Battery endurance | MISSING | None | Measure start/end voltage, duration, and workload |

## Latest Physical Observation

The final firmware was flashed after replacing sample-to-sample stillness with
a bounded candidate resting region. The user reported that operation was normal
and that no further firmware download was needed. The live dashboard screenshot
shows the final system in a static state with normal storage and active BLE.
On 2026-08-10, 420 sequential raw W25Q64 records were exported over USART1
without a missing sequence or malformed CSV row.
