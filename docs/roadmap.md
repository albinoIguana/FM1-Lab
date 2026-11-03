# FM1-Lab roadmap

## Priority 1 — Real FM-1 emulation
- [x] Pin the upstream MIT-licensed FM-1 emulator as a recursive Git submodule.
- [x] Build the upstream WebAssembly runtime and FM-1 SID pages in CI.
- [x] Run upstream Rust and browser tests.
- [x] Run the Felucca firmware smoke test and verify MIDI produces audio.
- [x] Verify flash persistence with the upstream WASM round-trip test.
- [x] Make the upstream FM-1 emulator the primary site at the root URL.
- [ ] Verify playback, touch controls and firmware selection on desktop and iOS Safari.
- [ ] Add a repeatable test matrix for selected firmware builds and saved-flash scenarios.
- [ ] Capture audio regression references and compare output across emulator changes.

## Priority 2 — Emulator development tools
- [x] Add same-origin boot-time script support for MIDI/key/encoder input scenarios.
- [x] Receive documented try-out lifecycle events with origin and iframe-source checks.
- [ ] Build a scenario runner for firmware, script and flash-overlay combinations.
- [ ] Explore a documented upstream host API for read-only telemetry and safe live parameter writes.
- [ ] Add a developer panel for emulator build, firmware version, audio health and test results.

## Priority 3 — Four-patch Instrument architecture
- [x] Prototype an Instrument as a preset containing four FM patches.
- [x] Prototype patch editor, operator map, macros and local six-operator DSP.
- [ ] Map the Instrument model to capabilities that the actual emulator/firmware exposes.
- [ ] Decide whether an Instrument should be four firmware presets, four sequenced parts, or another real FM-1 concept.
- [ ] Implement import/export only after the firmware data model and save format are understood.

## Experimental / deferred
The previous FM1-Lab Web Audio engine is not the stock FM-1 engine. Keep it as an isolated prototype until its parameters can be mapped to real firmware behavior with tests.
