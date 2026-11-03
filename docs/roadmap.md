# FM1-Lab roadmap

## Priority 1 — Real firmware foundations
- [x] Pin the upstream MIT-licensed FM-1 emulator as a recursive Git submodule.
- [x] Build the generic FM-1 emulator and smoke-test community firmware, LCD output, MIDI audio and flash persistence.
- [x] Publish the generic emulator as the compatibility bench.
- [x] Fetch Felucca source at a pinned commit in CI, including its nested engine and board dependencies.
- [x] Build Felucca's own browser-native firmware runtime with Emscripten.
- [x] Publish the Felucca runtime separately from the generic emulator.
- [ ] Verify Felucca browser runtime audio, controls and persistence on desktop and iOS Safari.
- [ ] Add repeatable Felucca runtime tests and retained regression artifacts.

## Priority 2 — FM1-Lab interaction and design language
- [x] Preserve the accepted FM1-Lab boot animation.
- [ ] Replace the generic dashboard feel with a hardware-oriented FM-1 control surface and screen hierarchy.
- [ ] Build a page/parameter navigation model around Felucca's real controls and parameter descriptions.
- [ ] Adapt Felucca's existing FX page and quick layer to the Clipper-inspired workflow: select, page, tweak, hear immediately; clear bypass.
- [x] Use Felucca's existing 32-algorithm FM6 topology graph, carrier/modulator/feedback roles and operator/envelope edit pages as the baseline.
- [ ] Extend role-specific macro controls in the FM1-Lab layer while retaining Felucca's tested operator editor.
- [ ] Add a bridge from the custom front end to the Felucca host runtime; use its exported host API rather than a separate Web Audio synth.

## Priority 3 — Four-patch Instrument
- [x] Prototype an Instrument as a preset containing four FM patches.
- [ ] Map the four patches to Felucca's four independent tracks/parts, validating voice behavior and parameter boundaries.
- [ ] Define a sound-only Instrument save/load format separately from full sequencer projects.
- [ ] Implement per-patch mute/solo, level, pan and selection controls.
- [ ] Add stacked/split/velocity-zone behavior only where the actual voice and MIDI model supports it.
- [ ] Implement reliable Instrument import/export and regression tests.

## Reference projects
- **Felucca** is the firmware and DSP foundation.
- **Clipper** is an interaction/workflow reference for effects and deep editing, not a code dependency.
- **FM-1 emulator** is the independent compatibility bench for running supported firmware packages unchanged.

## Deferred
The old FM1-Lab local Web Audio engine is an isolated prototype, not the Felucca or stock FM-1 sound engine. Do not build new features on top of it. Retain it only as a temporary visual/interaction reference until the Felucca host bridge is in place.
