# Archived Device Export

A real device export was captured over USART1 on 2026-08-10. The STM32 was
running the current firmware, and the records were read from the W25Q64 without
clearing or manually editing the device log.

| File | Command | Contents | Records |
| --- | --- | --- | ---: |
| `2026-08-10-serial-transport-log.csv` | `export` | Simplified session, time, temperature, motion, and shock table | 388 |
| `2026-08-10-serial-transport-log-raw.csv` | `export_raw` | Sequence, record type, time, temperature, and XYZ engineering table | 420 |
| `2026-08-10-serial-export.txt` | n/a | Capture settings, validation summary, and SHA-256 hashes | n/a |
| `2026-08-10-user-securecrt-export.txt` | `export` | Original user-provided SecureCRT text with CSV markers | 378 |
| `2026-08-10-data-consistency.md` | n/a | Cross-check of manual and automated exports | n/a |
| `2026-08-10-device-state.txt` | `status_json`, `cfg get`, `time` | Runtime and detector-configuration snapshot | n/a |
| `2026-08-10-development-export-metadata.csv` | n/a | Machine-readable provenance and limitation row | 1 |

Validation of the raw export found sequence numbers 1 through 420 with no
missing sequence, malformed row, or `NA` timestamp. The accumulated records
contain 31 boot markers, 163 periodic temperature records, 75 motion starts,
64 motion ends, and 87 shock events. Valid recorded temperatures range from
23.6 C to 31.6 C.

This is an unlabeled development/bench dataset accumulated across multiple
sessions. It demonstrates persistent logging and serial export, but it must not
be used to claim event-detection accuracy, transport performance, or calibrated
temperature accuracy. The unequal motion-start and motion-end counts are kept
unchanged as part of the raw evidence.

The recorded events came from hand-held movement and light impacts during
development. Motion and shock thresholds, confirmation times, cooldown, and
the temperature interval can be changed and saved through the command
interface. Those settings provide a basis for later experiments, not evidence
that the current prototype has been validated for cold-chain transport,
vehicle impacts, or other operating conditions.

The manual SecureCRT export is fully consistent with the first 378 rows of the
automated simplified export. The automated file was captured about ten minutes
later and contains ten additional periodic temperature records. See
`2026-08-10-data-consistency.md` for the comparison.

## Expected Device Export Schema

The simplified export columns are:

```text
session,timestamp,time_s,temp_c,motion,shock
```

- `session`: boot/session identifier reconstructed from log delimiters.
- `timestamp`: DS1302 calendar time, or `NA` for legacy records.
- `time_s`: session-relative STM32 uptime in seconds.
- `temp_c`: temperature in Celsius, or `NA` for event-only rows.
- `motion`: motion state/event value exported by the firmware.
- `shock`: shock state/event value exported by the firmware.

The engineering export columns are:

```text
sequence,type,timestamp,uptime_s,temp_c,x_mg,y_mg,z_mg
```

- `sequence`: persistent Flash record sequence.
- `type`: `BOOT`, `TEMP`, `MOTION_START`, `MOTION_END`, or `SHOCK`.
- `x_mg`, `y_mg`, `z_mg`: ADXL345 acceleration components in mg.

## Scope Boundary

The repository uses these archived files as evidence of storage and export
behavior. They do not measure detection accuracy. Measuring accuracy would
require synchronized labels, controlled test conditions, and suitable reference
equipment.

Raw exports should remain unchanged. Any cleaned or derived data should be kept
separately with its transformation documented.
