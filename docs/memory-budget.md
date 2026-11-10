# FM1-Lab memory budget audit

**Target:** M-VAVE FM1, 1 MiB SPI NOR and 512 KiB internal SRAM  
**Active build:** fm1-base + Felucca-BSP + pinned Felucca FM6 core  
**Measured build commit:** `94c4b014ddafea81d11e224aa8c0f00a6eac98be` (2026-11-10)  
**CI:** [Native FM1 build #814](https://github.com/albinoIguana/FM1-Lab/actions/runs/42838837224) — successful  
**Audit boundary:** linker/build verified; physical-device flashing and runtime qualification are separate.

## 1. Flash budget — current device build

The FM1 has 1,048,576 bytes of SPI NOR, but firmware does not own all of it. The active Felucca-family linker/loader gives the application a **581,564-byte XIP slot**.

| Flash measurement | Bytes |
|---|---:|
| Physical SPI NOR | 1,048,576 |
| Active application XIP slot | 581,564 |
| Latest `app.bin` | **310,380** |
| Remaining XIP headroom | **271,184** |
| XIP slot used | 53.4% |
| XIP slot free | 46.6% |

The measured image is from the latest successful device-target build on 2026-11-10, not the browser emulator's WASM file and not the compressed `.fwsc` update package. The CI log reports `app.bin 310380 / 581564 XIP-slot bytes`.

The earlier **453,921-byte** ceiling and **484,552-byte** payload came from the retired M-VAVE SDK/packager path. That packager rejected its own legacy policy limit; it did not measure the active fm1-base/Felucca XIP slot. It is not the current release-build limit and must not be used to calculate current headroom.

## 2. What is in the firmware image

The active app includes three generated voice-data headers:

| Embedded dataset | Voices | Encoded voice data |
|---|---:|---:|
| Current curated DX7 factory library | 128 (32 Instruments × 4 patches) | 24,960 bytes/chars |
| Previous DX7 library retained for exact-match migration | 128 | 24,960 bytes/chars |
| Previous Digitone library retained for exact-match migration | 124 (31 Instruments × 4 patches) | 24,180 bytes/chars |
| **Total embedded voice strings** | **380** | **74,100 bytes/chars** |

These are Z85-encoded strings in `app/fm1lab_dx7_data.h`, `app/fm1lab_dx7_legacy_data.h`, and `app/fm1lab_digitone_data.h`. All three headers are included by `app/app.c`; the current DX7 set seeds the 32-Instrument factory library, while the older datasets are referenced by startup migration checks so edited user Instruments can be preserved while untouched older factory slots are upgraded.

The 74,100-byte figure is the encoded source-string payload, not a separately measured linker-symbol total. The current CI memory report aggregates code and read-only constants into `.text`; it does not yet print per-symbol sizes. The full image measurement already includes these embedded strings. The curated 128-voice factory library is therefore **inside the 310,380-byte application image**, not an additional 128-patch allocation on top of it.

The latest ELF section totals show `.text` at 303,768 bytes. That section includes executable code and read-only data; the build does not currently split those categories in its printed report.

## 3. User-editable Instrument storage is separate from XIP

The app reserves a 12-sector, 4 KiB-per-sector pool at `0xF0000..0xFBFFF` (49,152 bytes) for five pooled objects:

- Four Instrument bank objects: 3,840 bytes each, eight Instruments per bank; 15,360 bytes of bank payload total.
- One versioned settings/insert sidecar: 1,704 bytes in the current V5 format.
- Each object is CRC-protected and the pool maintains current/previous copies with rotating sectors for power-fail safety and wear distribution. The 12 sectors are a shared A/B pool, not five fixed one-sector allocations; do not calculate spare capacity by simply subtracting payload bytes from 49,152.

The three small app objects (settings, panel calibration and BLE state) use their own A/B sectors at `0xEA000`, `0xEC000`, and `0xEE000`, respectively. Together those occupy the separate `0xEA000..0xEFFFF` app-object window. The app map preserves Felucca projects, samples, user preset banks, autosave, voice bank, global sectors and OTA staging as documented in [REBUILD_AUDIT.md](REBUILD_AUDIT.md).

**Important distinction:** factory patch source data is embedded in the XIP firmware image; user-editable 32-Instrument records live in the reserved pooled flash store. They are separate budgets and both fit their currently defined regions.

## 4. Latest linker section report

The same successful CI run reported:

| ELF section / check | Used | Capacity | Remaining |
|---|---:|---:|---:|
| `.text` | 303,768 B | Included in XIP slot | — |
| `.ram_text` | 3,556 B | 32,768 B | 29,212 B |
| `.data + .bss` | 68,472 B | 98,304 B | 29,832 B |
| `.pool` | 296,744 B | 344,064 B | 47,320 B |
| `.pool` after mandatory 8 KiB reserve | — | — | 39,128 B |
| `.noinit` | 1,836 B | 15,696 B | 13,860 B |
| `app.bin` | **310,380 B** | **581,564 B** | **271,184 B** |

The application image is exactly `.text + .ram_text + .data` (303,768 + 3,556 + 3,056 = 310,380 bytes). The builder separately checks the 96 KiB `.data + .bss` region, the 32 KiB RAM-text region, and the 336 KiB pool while preserving at least 8,192 bytes of pool headroom.

The CI build expands the platform's 240 × 144 graphics canvas to 240 × 180 before compiling. The resulting 86,400-byte 16-bit canvas and 196,608 bytes of shared stereo FX feedback storage account for 283,008 bytes of the 296,744-byte pool total. The remaining 13,736 bytes cover other pool allocations, including migration and runtime buffers.

## 5. Conclusion and remaining measurement

- **32 Instruments / 128 current factory voices:** retained; no reduction is indicated by the current flash measurement.
- **Firmware flash:** the latest successful build has **271,184 bytes of XIP headroom** after embedding the current factory library and the legacy migration datasets.
- **User storage:** four 3,840-byte banks plus the 1,704-byte sidecar are in the dedicated 12-sector pool, separate from the application image.
- **No flash-map changes are warranted** based on this audit. Do not repurpose OTA staging, Felucca data regions, or device-reserved sectors.
- The remaining useful precision improvement is a linker map / symbol-size report that attributes the 303,768-byte `.text` section among executable code, the current DX7 data, and legacy migration data. The current build proves total fit, but does not yet provide that per-symbol breakdown.

This audit verifies the successful CI device-target link and configured flash layout. It does not claim physical-device flashing, OTA/recovery qualification, or runtime audio qualification.
