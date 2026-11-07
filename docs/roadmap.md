# FM1-Lab finish line

Status reviewed 2026-11-07 against the active branch
`feature/native-instrument-macro-workflow`.

This document replaces the earlier build-up checklist. It separates implemented product behavior from the small number of release blockers. The goal is a playable four-layer FM instrument that is ready for a deliberate UI-design pass — not a new synthesis engine.

## Product definition (frozen)

FM1-Lab is a four-layer FM instrument built around the existing Felucca six-operator FM6 engine and the fm1-base device platform.

- One Instrument contains four independent six-operator patches.
- Each patch has its own voice, level, pan, enable-by-zero-level behavior, and four non-destructive sound macros.
- The shared audio path provides stereo FX sends/returns, Instrument-wide Amp/Drive and three-band parametric EQ, stereo-linked master compression, and a final safety limiter.
- Performance features are a two-lane 64-step sequencer and a held-note arpeggiator.
- The physical FM1 panel is the primary interface. The browser emulator is a development and UI-review aid.
- Do not replace the FM6 engine, add another synthesis engine, add a sampler, or broaden the feature set during the finish pass.

## Implemented and covered by native tests

- [x] Four-patch Instrument model and native FM6 voice integration.
- [x] Four independent patch voice pools and layer gain/mute.
- [x] Algorithm-aware Brightness, Body, Motion and Edge macro transformations.
- [x] Operator envelope, oscillator and global LFO editing APIs.
- [x] Two-lane sequencer, lane-to-patch assignment, transport and step editing.
- [x] Held-note arpeggiator.
- [x] Four stereo FX buses with per-patch send controls.
- [x] Instrument-wide Amp/Drive and three-band parametric EQ.
- [x] Stereo-linked master compressor and final peak limiter.
- [x] Versioned Instrument storage, CRC validation, legacy migration paths and separate versioned master-insert state.
- [x] Versioned performance settings for sequencer patterns, tempo, lane assignment, arp mode, FX controls/sends and compressor settings.
- [x] Native regression coverage for voice allocation, sequencer timing, arp output, stereo FX, send isolation, master inserts, linked compression, limiter bounds and render chunk determinism.
- [x] Device firmware build workflow passes.
- [x] Native browser-emulator build and bundle verification pass. Latest verified emulator build: [run 42360183444](https://github.com/albinoIguana/FM1-Lab/actions/runs/42360183444). Latest native regression checks: [run 42360183470](https://github.com/albinoIguana/FM1-Lab/actions/runs/42360183470), passed at `57ae14b2`. Latest device package build: [run 42360183743](https://github.com/albinoIguana/FM1-Lab/actions/runs/42360183743), passed at `57ae14b2`. All three workflows pass on the same source revision. Its downloadable artifact is attached to that run; the generated files are also published under `native-emulator/` on this feature branch.

## Latest device build audit

The latest passing native FM1 package build reports:

| Resource | Used | Limit |
|---|---:|---:|
| XIP image | 189,124 B | 581,564 B |
| RAM data + BSS | 47,336 B | 98,304 B |
| RAM text | 3,556 B | 32,768 B |
| Audio/object pool | 275,264 B | 344,064 B |

The `.ram_text` audit reports 1,123 instructions and no calls. The firmware package is `app/build/fm1lab.fwsc`; SHA-256: `580fb5d5a1af36dc8a5d6fd109cbd8b238eaeb83f72b9d364ccdcfe12a0d6176`. The CI artifact ZIP checksum was independently verified after download. These are package/build checks, not physical-device playback tests.

### Emulator runtime smoke check

The latest WebAssembly emulator boots, draws the 240×240 framebuffer and navigates HOME, MACRO, GLOBAL, OPERATOR, ENV, both LFO pages, both sequencer lanes, ARP and all eight FX pages. A held F3 key produces non-zero stereo output across all four enabled patches, with silence before note-on and a release tail after note-off. The generated proof-of-life render is 2.99 seconds at 44.1 kHz. This verifies the emulator audio path, not the physical FM1 output stage.
## Finish pass — no new feature scope

### P0: make the current instrument trustworthy

- [x] Run the complete native test workflow against the current branch head after the emulator publication commit.
- [x] Verify the device package contents, linker XIP/RAM report and artifact checksum.\n- [x] Audit the storage object map: settings/panel/BLE occupy EA000/EC000/EE000; four 3,840-byte bank records plus the 936-byte insert journal are pooled in F0000–FBFFF. Bank records are 8-byte headers plus eight 479-byte Instrument slots; the sidecar uses CRC-sealed active/fallback snapshots and bank CRC/generation matching.
- [x] Prevent boot-time factory formatting of present-but-invalid or unexpected-size Instrument objects; malformed user banks are preserved and reported instead of silently overwritten (commit `5e3f3760`).
- [ ] Add or complete host tests for the physical-control-to-page mapping and the high-risk save/load/navigation paths.
- [ ] Complete the UI event queue audit and route FX/sequencer/arp/tempo control writes through the audio-owner queue. The current audit found those setters are still called directly from the UI loop while the render path consumes the same state. Also test overflow recovery and stale voice snapshots.
- [ ] Verify first-boot factory initialization, Instrument switching, dirty-edit save behavior, and settings restore with a host-level `app_init` harness; storage codec tests already cover bank CRC, legacy sidecar migration, and journal-first power-loss recovery.
- [ ] Confirm every visible control changes the parameter named on screen; remove any inert or misleading label.

### P1: UI design handoff

- [x] Produce a page-by-page control matrix: physical button/encoder → action → displayed feedback → persistence owner.
- [x] Review every 240×240 screen in the emulator at native size; check clipping, hierarchy, labels, page numbering and selected-patch identity.
- [x] Lock a consistent page shell and 2×2 panel geometry before visual redesign.
- [x] Make HOME, macro, operator, ENV/LFO, sequencer and FX pages internally consistent.
- [ ] Verify navigation is predictable and reversible; HOME always returns to the Instrument overview.
- [ ] Mark the design handoff complete only when the user can start visual UI design without having to redesign the underlying interaction model.

### P2: physical FM1 validation (cannot be completed in CI)

- [ ] Flash and boot on an actual FM1 using a recoverable update path.
- [ ] Verify keybed velocity/aftertouch, USB/TRS/BLE MIDI, note-off/panic and patch switching.
- [ ] Listen to all four layers, pan, macros, FX, EQ, compressor, sequencer and arp.
- [ ] Measure device CPU load and xrun/dropout behavior under the documented 32-voice / two-lane / arp / all-FX stress case.
- [ ] Verify version-1 bank migration, version-2 writes, power-loss recovery and settings persistence on real flash.
- [ ] Only call the firmware flash-ready after these checks pass.

## UI design pass (next after the P0 reliability fixes)

The current 240×240 pages are a functional control/layout scaffold, not final visual design. Once the audio-owner queue and host save/load tests are closed, begin the dedicated UI pass: define the visual system, improve hierarchy and state visibility, refine encoder interaction feedback, and iterate against the native-size emulator screenshots. Keep the tiny display and physical controls as the design constraints; do not add new features.

## Interface contract

### Physical buttons

| Button | Action |
|---|---|
| HOME | Return to the four-layer overview |
| SEL | Toggle the selected patch's macro page / HOME |
| EDIT/SCL | Open the operator editor; from the operator editor, return to macros |
| ENV | Open operator envelope; press again to switch rates / levels |
| LFO | Toggle oscillator and modulation pages |
| GLO | Toggle global settings / HOME |
| FX | Advance through the eight master/effect pages |
| SEQ | Open the two-lane sequencer |
| PLAY | Start/stop when the sequencer page is open |
| ARP | Open the arpeggiator and cycle OFF / UP / DOWN / UP-DOWN |
| SAVE | Save Instrument, inserts and performance settings |
| OCT- / OCT+ | Transpose the keybed |
| REC | Bluetooth action when that platform feature is built |

### Encoders

| Encoder | Context | Action |
|---|---|---|
| SELECT | HOME and instrument pages | Select one of four patches |
| SELECT | FX pages | Cycle the eight FX pages |
| SELECT | Operator editor | Select OP1–OP6 |
| SELECT | Sequencer | Select one of 64 steps |
| ALGO | HOME | Select bank |
| ALGO | Operator editor | Select FM algorithm 1–32 |
| ALGO | EQ / ENV | Select EQ band / envelope operator |
| ALGO | Sequencer | Assign lane patch |
| ALGO | Compressor | Adjust makeup |
| ALGO | FX routing | Select patch whose sends are edited |
| PRESETS | Sequencer | Select lane A/B |
| PRESETS | Operator editor | Toggle K4 between fine frequency / detune |
| PRESETS | Other pages | Select Instrument slot within current bank |
| K1–K4 | HOME | Patch 1–4 level; zero is OFF |
| K1–K4 | Macro | Brightness / Body / Motion / Edge |
| K1–K4 | Operator | Output / mode / coarse / fine-or-detune |
| K1–K4 | ENV / LFO | Four parameters shown on the page |
| K1–K4 | Sequencer | Step enable / note / velocity / gate |
| K1–K2 | ARP | Tempo / mode |
| K1–K4 | FX pages | Effect parameters documented below |

- HOME: SELECT chooses one of four patches; K1–K4 control patch 1–4 volume independently; zero means off.
- SEL: toggles the selected patch's four-macro page.
- EDIT/SCL: opens the operator editor. SELECT chooses OP1–OP6, ALGO edits the patch algorithm, PRESETS toggles the K4 pitch field between fine frequency and detune; K1 output, K2 ratio/fixed mode, K3 coarse frequency and K4 fine/detune are live voice edits. The carrier/modulator roles update from the selected algorithm.
- ENV and LFO: open their dedicated operator envelope and LFO pages.
- FX: advances through the eight FX pages; SELECT cycles within that section.
- On master Amp/Drive, EQ and compressor pages, B_EDIT toggles bypass. EQ ALGO chooses LOW/MID/HIGH band. Compressor ALGO controls makeup.
- FX pages: K1 controls wet mix; K2 controls chorus/phaser rate, delay time or reverb decay; K3 controls feedback except phaser depth; K4 controls the selected patch's send except phaser resonance. B_EDIT toggles the selected effect. The dedicated SEND ROUTING page edits the four sends for the patch selected with ALGO.
- SEQ: opens the two-lane sequencer; PRESETS selects lane A/B, ALGO assigns that lane's patch, SELECT chooses the step, K1–K4 edit enable/note/velocity/gate, and PLAY starts/stops it.
- ARP: opens/cycles arp modes; held keybed notes are the input.
- SAVE: persists the current Instrument and performance settings.

## Release guardrails

- Keep all work on `feature/native-instrument-macro-workflow`; do not modify `main` or merge without explicit approval.
- No new synthesis engine or feature expansion during the finish pass.
- A passing host test or emulator build is not evidence of physical hardware audio performance.
- Be explicit about what is verified in CI, what is verified in the emulator, and what still requires a physical FM1.
