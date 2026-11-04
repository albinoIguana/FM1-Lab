# FM1-Lab implementation roadmap

Status reviewed 2026-11-04 against the agreed physical FM1 firmware target.

## Browser-side foundation

- [x] Use M-VAVE's official FM1 source and SDK as the synthesis foundation.
- [x] Pin the official source as a Git submodule.
- [x] Define one Instrument as exactly four independent six-operator DX7 patches.
- [x] Store each voice in M-VAVE's expanded 156-byte DX7 format.
- [x] Implement non-destructive Brightness, Body, Motion and Edge macro transforms.
- [x] Derive carrier roles from M-VAVE's actual DX7 algorithm routing table.
- [x] Add browser-side operator editing for oscillator mode, coarse/fine, output and four-stage envelopes.
- [x] Connect the browser workshop to M-VAVE's official simulator and audio worklet.
- [x] Verify the browser engine build and required WASM exports in GitHub Actions.

## Native firmware core — started

- [x] Add a C Instrument data contract: exactly four native 156-byte voices.
- [x] Add per-patch macro values, enabled/mute state and level state to the native model.
- [x] Add native carrier-mask lookup derived from the official M-VAVE algorithm routing table.
- [x] Add non-destructive native macro rendering and host-side C unit tests.
- [x] Define and test a CRC-protected 400-byte metadata block for eight named Instruments (32 patches) in the unused tail of each 4 KiB voice-bank sector.
- [ ] Integrate the native model with the FM1 device app and actual voice engine.
- [ ] Connect patch enable and level state to the device mixer / voice routing.

## Device storage and controls

- [x] Add a version-2 bank format and metadata read/write API to the existing four-bank DX7 voice library; preserve version-1 bank reads and stage/recovery flow.
- [ ] Verify version-1 to version-2 upgrade, metadata persistence, and interrupted-write recovery on the device flash implementation.
- [ ] Store named four-patch Instruments and macro state; preserve existing project slots and user data.
- [ ] Map the physical preset encoder to Instrument selection.
- [ ] Map physical controls 1–4 to the four patches.
- [ ] Implement the HOME Instrument screen with four patch tiles, names, active state and selection.
- [ ] Add four macro controls with algorithm-aware carrier/modulator behavior.
- [ ] Complete the deep six-operator editor and immediate FX controls on-device.
- [ ] Add Instrument rename, duplicate, save/load and import/export behavior.

## Firmware build and hardware validation

- [ ] Integrate the FM1-Lab C module into the native device build.
- [ ] Build with the matching JieLi Windows toolchain 2.5.3.
- [ ] Produce and inspect a device firmware image without touching the live/main branch.
- [ ] Test patch isolation, macro limits, mute/level, save/load integrity, MIDI and rollback on actual FM1 hardware.
- [ ] Only call the firmware flashable after the device build and hardware checks pass.

## Guardrails

- Browser simulation is a development and audition aid, not the deliverable.
- Do not overwrite the existing 16 project slots or legacy DX7 banks without a compatibility plan.
- Keep all work on `feat/m-vave-native-instrument` until the user explicitly approves a merge.
- A passing browser/WASM build is not evidence of a flashable device firmware image.
