# Serial Export Consistency Check

## Sources

1. User SecureCRT capture:
   `2026-08-10-user-securecrt-export.txt`
2. Automated USART1 simplified export:
   `2026-08-10-serial-transport-log.csv`
3. Automated USART1 engineering export:
   `2026-08-10-serial-transport-log-raw.csv`

## Result

The user capture contains one live ADXL345 status line before `CSV_BEGIN`, then
a valid simplified CSV block ending at `CSV_END`. The status line is normal
asynchronous console output and is not part of the CSV.

| Check | User capture | Automated simplified export |
| --- | ---: | ---: |
| Data records | 378 | 388 |
| Malformed CSV rows | 0 | 0 |
| Reconstructed sessions | 27 | 27 |
| Motion starts | 75 | 75 |
| Motion ends | 64 | 64 |
| Shock events | 87 | 87 |
| Last timestamp | 2026-08-10 00:48:41 | 2026-08-10 00:58:47 |

All 378 user-captured records are byte-for-byte equal after line extraction to
the first 378 records of the automated simplified export. The automated export
was taken later and contains ten additional periodic temperature rows from
00:49:42 through 00:58:47. Those ten rows contain no new motion or shock event.

The engineering export contains 420 records with sequence 1 through 420,
without a missing sequence or malformed row. It includes 31 `BOOT`, 163
`TEMP`, 75 `MOTION_START`, 64 `MOTION_END`, and 87 `SHOCK` records. All 420
records have valid RTC timestamps.

## Interpretation Boundary

This check establishes that the manual and automated serial exports represent
the same stored log history and that later recording explains their length
difference. It does not establish whether every detected event corresponds to
a labelled physical event, because no synchronized ground-truth annotation was
collected during these development sessions.
