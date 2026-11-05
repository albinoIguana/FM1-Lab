# FM1-Lab native rebuild audit

## Hardware budget (verified against the community hardware map)

- FM1 SPI NOR flash: 1 MiB.
- Documented Felucca-family XIP application region: `0x02000120`, length `0x8DFBC` = **581,564 bytes**.
- Internal SRAM used by firmware: 512 KiB address window (`0x01C00000..0x01C7FFFF`); the top region `0x01C7FD50..` is reserved for boot information, update mailbox and vectors.
- Stock flash map:
  - `0x00000..0x03FFF`: boot area.
  - `0x04000..0x92FFF`: firmware.
  - `0x93000..0xE8FFF`: VM/config store.
  - `0xE9000..0xE9FFF`: Bluetooth pairing.
  - `0xEA000..0xFBFFF`: user patches/resources.
  - `0xFF000..0xFFFFF`: unit reservation.
- Firmware families intentionally relocate or share data areas. Never treat gaps or another firmware's data ranges as free. Every persistent record needs a magic, version and CRC, and update staging must be reserved.

## Previous FM1-Lab build: corrected diagnosis

From the last device link diagnostics:
- Application payload: **484,552 bytes**.
- Felucca-family XIP slot: **581,564 bytes**.
- Headroom against that XIP slot: **97,012 bytes**.
- The stock M-VAVE packager rejected the image because its own reserved-space calculation allowed only 453,921 bytes; the image was 30,631 bytes above that policy ceiling.

That rejection did **not** prove the image exceeded the FM1's physical application slot. The fix is not to blindly change a constant in the stock packager. Use a proven linker, flash map, update loader and package builder as one consistent system, then enforce the 581,564-byte XIP boundary in that system.

## Verified Felucca-compatible storage map

The fm1-base package builder is not the stale generic layout described in one comment in `device/storage.c`. Its pinned BSP defines the actual writable windows:
- XDATA1: `0xE7000..0xFBFFF`; Felucca uses `0xE7000` for its voice bank.
- OTA staging: `0xE0000..0xE4FFF` (five sectors), not the entire `0xE0000..0xFBFFF` range.
- Autosave: `0xE5000..0xE6FFF`.
- Global sectors: `0xFC000..`; Felucca uses `0xFC000/0xFD000` for settings and `0xFE000` for the second FM6 bank copy.

Felucca's existing data map is:
- Projects: `0x97000..0x9EFFF`.
- FM6 patch bank A/B: `0x9F000` and `0xFE000`.
- User samples: `0xA0000..0xDBFFF`.
- User preset banks: `0xDC000..0xDFFFF`.
- OTA staging: `0xE0000..0xE4FFF`.
- Autosave: `0xE5000..0xE6FFF`.
- FM6 voice bank: `0xE7000`.
- Felucca globals/settings: `0xFC000..`.

The fm1-base template's default map (`0xFC000`, `0xDC000`, `0xDE000`) would collide with Felucca globals and user preset banks. This rebuild explicitly relocates its three small app objects to `0xEA000..0xEFFFF` and reserves the 12-sector pool `0xF0000..0xFBFFF` for our Instrument bank. These lie in the package's preserved USER range and BSP XDATA1 window; they do not overlap Felucca projects, samples, user preset banks, autosave, voice bank or FM6 bank copies. The old stock USER patch area is repurposed for FM1-Lab app objects; Felucca's project and preset banks are preserved. The OTA staging window remains untouched.

The BSP's OTA implementation and flash-window definitions are the authority here: `chip/ota.c` defines five staging sectors at `0xE0000`, and `chip/fm1_flash.h` defines XDATA1 as `0xE7000..0xFC000`. The broader staging comment in fm1-base's storage source is stale and is not used as the flash map.

## Instrument storage sizing

The persisted record is 607 bytes: four 128-byte FM6 voice records, four macro values per patch, Instrument name/enable mask, and the two 16-step note/velocity lanes plus tempo. Six records plus a 12-byte chunk header use 3,654 bytes, below the 3,840-byte payload limit per storage sector. Sixteen Instruments are split 6/5/5 across three pooled objects. The 12-sector pool provides two-copy atomic storage for the three objects with six spare sectors for rotation/garbage collection.

The three platform objects use six sectors (`EA000..EFFFF`); the instrument chunks use the remaining twelve sectors (`F0000..FBFFF`). This consumes the full 18-sector USER window. Felucca project/sample/preset/voice-bank/global sectors remain outside this allocation. Stock-firmware USER patches in `EA000..FBFFF` are not preserved; Felucca's projects and user preset banks are.

The first native build produced a 151,428-byte app image. After adding the 16-slot Instrument bank, the app image measured 155,172 bytes and RAM `.data+.bss` measured 54,168 of 98,304 bytes. Those numbers predate the two-lane sequencer and current UI work; the latest build must remeasure them and run the host audio regression suite.

## Component selection

| Layer | Selected source | Use |
|---|---|---|
| Hardware/BSP, linker, loader, packaging | [felucca-bsp](https://github.com/hugelton/felucca-bsp), pinned through [fm1-base](https://github.com/charlesvestal/fm1-base) | Known FM1 memory map, update path and packaging |
| Platform/UI primitives, panel, audio, MIDI, storage, host simulator | [fm1-base](https://github.com/charlesvestal/fm1-base) | Small optional platform groups; native host simulator and common app interface |
| Minimal hardware reference | [Dinghy](https://github.com/zvenson/dinghy) | Cross-check boot, USB rescue, MIDI, audio, storage and safety checks; do not carry its sine-only app |
| Six-op FM reference | [Felucca](https://github.com/hugelton/Felucca) | Integer-only MSFA/Dexed-derived FM6 core and patch semantics; use its core and required generated tables, not the browser DX7 simulator |
| Sequencer | FM1-Lab native 2-lane/16-step scheduler; patterns stored per Instrument | Minimal scheduler and shared clock/gate; no Felucca or Baud Girl sequencer code copied |
| Product UI | FM1-Lab Instrument/editor design, reimplemented against the platform graphics API | Four-patch Instrument, algorithm-aware editor, macros and two visible sequencer lanes |

## Open-source project comparison

| Project | What it offers | Decision for this build |
|---|---|---|
| [fm1-base](https://github.com/charlesvestal/fm1-base) + [felucca-bsp](https://github.com/hugelton/felucca-bsp) | Reusable AC79 hardware/BSP, linker, loader, package builder, audio/MIDI/storage/UI primitives and host simulator; configurable optional features. The documented app slot is 581,564 bytes, with 96 KiB app RAM and 336 KiB memory pool. | **Platform selected.** Smaller and cleaner than importing a complete groovebox; own BLE stack avoids linking the closed JieLi Bluetooth libraries. |
| [Felucca](https://github.com/hugelton/Felucca) | Complete four-track, multi-engine firmware with 13 engines and a 64-step piano-roll/song sequencer. FM6 is its six-operator integer FM engine. | **FM6 core + required patch conversion/table generator only.** Do not import its other 12 engines, song system, sampler, or full UI. The released full app binary is about 513 KB; our first native image was about 151 KB before the current Instrument/sequencer work. |
| [Dinghy](https://github.com/zvenson/dinghy) | Minimal documented firmware with a four-voice sine synth; useful for understanding boot, USB rescue, MIDI, audio and storage plumbing. | Reference only. A sine engine is not the six-operator instrument we want, and fm1-base gives us a more complete platform layer. |
| [SLOOP](https://github.com/3dSam/SLOOP) | Felucca-derived four-track groovebox with three synth parts plus drums and live-performance features. | Not imported. It carries more engine, sequencing and performance functionality than this single-Instrument design needs. |
| [M-VAVE FM1 Open](https://github.com/m-vave/fm1_open) | Official eight-track app with sample slots and effects; its app is open, but its device-engine SDK is a binary SDK with a linking exception. | Reference only. A binary engine is not a suitable source base for our custom FM6 algorithm editor and topology-aware macros. |
| Baud Girl FM-1+VA | Closed-source firmware with the stock FM engine, VA/chiptune additions and a 64-step sequencer. | Not copyable as source. We implement only the agreed two-lane/16-step sequencer ourselves; this is not a port of Baud Girl. |
| fm1-nes / foma | Alternative SDK/minimal-app experiments. | Not selected: they do not offer a better combination of native FM6 source, platform services and verified packaging than fm1-base + Felucca FM6. |

This is a component-level selection, not a wholesale fork: fm1-base owns the hardware boundary, Felucca supplies the FM6 DSP, and FM1-Lab owns the Instrument model, macro system, two-lane sequencer, storage format and UI.

## Build and runtime principles

1. Build the unmodified fm1-base template first, using its pinned BSP and documented JieLi toolchain.
2. Start with the optional feature set actually required. Bluetooth MIDI, if enabled, uses fm1-base's own stack so the GPL FM6/sequencer sources do not link JieLi's closed Bluetooth libraries. No Wi-Fi, Bluetooth audio, sampler, or unused engines by default.
3. Use the integer-only FM6 core. Do not use the browser simulator's floating-point DX7 engine in the device build.
4. Implement four patch slots under one Instrument and a shared, measured voice budget. Do not instantiate four independent oversized synths.
5. Implement only the agreed two-lane sequencer; share transport/clock and keep timing variations out of scope.
6. Keep DSP in the audio render path and UI/storage work outside it. Test audio determinism, note-off safety, CPU budget, RAM and pool headroom.
7. The packager must enforce the actual linker XIP region, while the linker and storage map preserve the bootloader, loader staging, VM, pairing and user data.
8. No release or main-branch merge until the device package, host tests, memory checks and update/recovery checks all pass. Hardware audio remains unverified until tested on a physical FM1.

## Licensing / provenance

- fm1-base and felucca-bsp: MPL-2.0.
- Felucca FM6 engine wrapper and patch/table generator: GPL-3.0-only. The Felucca sequencer and other engines are not copied.
- Felucca `fm6_core.c`: Apache-2.0; retain its header and Apache license notice.
- Dinghy: GPL-3.0-only; reference only unless source is copied, in which case retain its notices.
- The firmware retains per-file notices and an explicit third-party license/credits inventory. The FM1-Lab sequencer and Instrument storage codec are original project code. The own-stack Bluetooth choice avoids linking JieLi's closed Bluetooth libraries into the GPL application.

## Preserved work

The prior implementation branches remain unchanged as audit/reference snapshots. This clean native firmware rebuild proceeds on `rewrite/fm1-native-instrument`; it does not touch main or the live site.
