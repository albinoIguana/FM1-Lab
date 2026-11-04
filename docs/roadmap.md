# FM1-Lab implementation roadmap

Status reviewed 2026-11-04 against the agreed physical FM1 firmware target.

## Architecture boundaries

- [x] Add an InstrumentSession application service between the data model and M-VAVE engine; centralize topology-aware rendering and serialize mutable-track writes.
- [x] Extract HOME patch-grid rendering into a pure, independently tested view module.
- [ ] Extract patch-editor and reusable control rendering into independent UI modules before expanding the screen set.

## Two-track sequencer foundation

- [x] Define exactly two user-facing lanes on independent 64-step timelines.
- [x] Align active lengths to native 16-step increments (16/32/48/64), shared tempo (30–300 BPM), and shared swing (0–50).
- [x] Align step fields to native note, velocity, gate, tie and alternate-tuning data; remove unsupported per-track division/swing and slide controls.
- [x] Add the combined two-lane SEQ screen, patch assignment, and 24-row piano-roll editing surface.
- [x] Add a native C adapter for two explicit normal DX7 tracks, including tempo, shared swing, loop length, note/gate/tie/alternate-tuning edits, lane selection and transport.
- [x] Add a runtime-only native playback mask so the two bound lanes play while the other six project tracks remain stored and untouched; unbind restores all eight.
- [x] Wire the physical two-lane device screen to the adapter, with a central exit hook that stops transport and restores all eight project tracks.
- [ ] Verify project-load rebinding, clock/transport and save/load behavior on the device.

## Browser-side foundation

- [x] Use M-VAVE's official FM1 source and SDK as the synthesis foundation.
- [x] Pin the official source as a Git submodule.
- [x] Define one Instrument as exactly four independent six-operator DX7 patches.
- [x] Store each voice in M-VAVE's expanded 156-byte DX7 format.
- [x] Implement non-destructive Brightness, Body, Motion and Dirt macro transforms.
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
- [x] Add a native library adapter that loads/saves four consecutive DX7 voices as one Instrument and carries its name, macro values, enabled state and level.
- [x] Add a live Instrument runtime for PRESETS selection, four patch selection, enable/mute, level and macro editing, with unsaved-edit protection and no flash I/O in the render accessor.
- [x] Add a native six-operator voice-editing API with verified OP1–OP6 byte mapping, bounded envelope/output/mode/ratio/detune fields, global algorithm/feedback/LFO fields, and topology-aware carrier/depth queries.
- [x] Add the first native six-operator patch-editor screen, editing the selected Instrument patch directly; expose algorithm-aware carrier/modulator roles and depth, envelope rates/levels, oscillator output/ratio/detune, global feedback/LFO, patch selection and Instrument save.
- [ ] Add a true operator connection graph, undo/compare, and patch audition/voice-engine integration; the current topology strip reports roles/depth but does not yet draw every routing edge.
- [x] Save the four voice records and metadata together through one version-2 bank transaction, preserving the other seven Instruments in that bank.
- [ ] Integrate the native model with the FM1 device app and actual voice engine.
- [ ] Connect patch enable and level state to the device mixer / voice routing.

## Device storage and controls

- [x] Add a version-2 bank format and metadata read/write API to the existing four-bank DX7 voice library; preserve version-1 bank reads and stage/recovery flow.
- [ ] Verify version-1 to version-2 upgrade, metadata persistence, and interrupted-write recovery on the device flash implementation.
- [x] Map 32 named four-patch Instruments across four existing DX7 banks (8 Instruments / 32 patches per bank); preserve existing project slots and unrelated bank entries.
- [x] Verify and name the printed panel controls (MASTER, SELECT, ALGORITHM, KNOB 1–4, PRESETS) against upstream scan indices.
- [x] Replace numeric native screen encoder references with named physical-control constants in the simulator source build.
- [ ] Map PRESETS to the new four-patch Instrument selector and KNOB 1–4 to patch slots in the redesigned HOME screen.
- [ ] Implement the HOME Instrument screen with four patch tiles, names, active state and selection.
- [ ] Add four macro controls with algorithm-aware carrier/modulator behavior.
- [ ] Complete the deep six-operator editor and immediate FX controls on-device.
- [ ] Add Instrument rename, duplicate, save/load and import/export behavior.

## Firmware build and hardware validation

- [x] Add a reproducible Windows entrypoint that stages the FM1-Lab C modules and patches into the pinned native app tree before invoking the SDK firmware-only build.
- [ ] Integrate the Instrument model with device note allocation, mixer routing and HOME UI in the native device build.
- [ ] Build with the matching JieLi Windows toolchain 2.5.3.
- [ ] Produce and inspect a device firmware image without touching the live/main branch.
- [ ] Test patch isolation, macro limits, mute/level, save/load integrity, MIDI and rollback on actual FM1 hardware.
- [ ] Only call the firmware flashable after the device build and hardware checks pass.

## Guardrails

- Browser simulation is a development and audition aid, not the deliverable.
- Do not overwrite the existing 16 project slots or legacy DX7 banks without a compatibility plan.
- Keep all work on `feat/m-vave-native-instrument` until the user explicitly approves a merge.
- A passing browser/WASM build is not evidence of a flashable device firmware image.
