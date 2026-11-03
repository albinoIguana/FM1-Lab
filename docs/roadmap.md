# FM1-Lab accelerated roadmap

## Sprint A — Foundation and playable workbench
- [x] Responsive UI, startup and operator editor.
- [x] Six-operator design model and patch persistence/export.
- [x] Real-time Web Audio FM voice.
- [x] Six oscillators, three selectable FM routing graphs, carrier summing.
- [x] Per-operator attack, decay, sustain and release envelopes.
- [x] Delayed feedback loops and four performance macros.
- [x] Add zero-dependency mocked audio-graph tests and GitHub Actions CI.
- [ ] Browser/device smoke tests, accessibility and GitHub Pages deployment.

## Sprint B — Emulator integration proof
- [x] Inspect upstream WASM/UI and try-out source boundaries.
- [x] Embed official emulator using documented `?embed=1` mode.
- [x] Document same-origin and cross-origin constraints.
- [ ] Test emulator iframe on desktop and iOS Safari.
- [ ] If desired, propose a secure upstream host API rather than relying on private internals.

## Sprint C — FM engine v1
- [x] Modular local DSP engine in `src/fm-engine.js`.
- [x] Six independently configurable operators.
- [x] Three genuine audio-rate modulation topologies.
- [x] Carrier summing, per-operator ADSR and delayed feedback.
- [x] Brightness/body/motion/feedback macros.
- [ ] Add held-note note-off and polyphonic voice stealing.
- [ ] Normalize modulation index by algorithm and operator level.
- [ ] Add oversampling/aliasing evaluation, audio regression tests and CPU profiling.
- [x] Add malformed-patch guard and mocked routing/feedback tests.
- [ ] Add canonical patch schema validation and migration tests.

## Combined sprint exit criteria
A stable standalone workbench that produces six-operator FM sound, exports patches, and embeds the official emulator without misrepresenting integration capabilities. Code syntax and mocked graph construction can be checked in CI; real browser/audio testing remains required.

## Later
Algorithm-aware macro mapping, additional algorithms, four-layer hybrid architecture, sequencer/arpeggiator, automation, effects, presets and hardware compatibility matrix.
