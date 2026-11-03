# FM1-Lab accelerated roadmap

## Sprint A — Foundation and playable workbench
- [x] Responsive UI, startup and operator editor.
- [x] Six-operator design model and patch persistence/export.
- [x] Real-time Web Audio FM voice.
- [x] Six oscillators, three selectable FM routing graphs, carrier summing.
- [x] Per-operator attack, decay, sustain and release envelopes.
- [x] Delayed feedback loops and four performance macros.
- [x] Add zero-dependency mocked audio-graph tests and GitHub Actions CI.
- [ ] Browser/device smoke tests and accessibility review.\n- [x] Publish static app through GitHub Pages with automatic deployment.

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
- [x] Add held-note note-off and bounded polyphonic voice stealing.
- [ ] Normalize modulation index by algorithm and operator level.
- [ ] Add oversampling/aliasing evaluation, audio regression tests and CPU profiling.
- [x] Add malformed-patch guard and mocked routing/feedback tests.

## Sprint D — Instrument architecture
- [x] Define Instrument as a saved preset containing exactly four FM patches.
- [x] Add four factory Instruments and four-patch home screen.
- [x] Add preset selector and four patch-select controls.
- [x] Reuse the six-operator editor for the selected patch.
- [x] Persist Instruments independently and export the complete Instrument as JSON.
- [x] Migrate legacy single-patch local saves into the new four-patch structure.
- [x] Add Instrument schema and migration tests.
- [ ] Browser/device smoke tests for preset switching, patch editing, persistence and export.

## Sprint E — Firmware test console
- [x] Add a dedicated full-interface FM-1 emulator console page.
- [x] Embed the upstream emulator's full firmware selection, local package loading, controls, MIDI input and save/backup UI.
- [x] Document what the console can test and the cross-origin automation limits.
- [x] Deploy the app to GitHub Pages after approval.
- [ ] Explore a safe same-origin scripted test runner or upstream host API for repeatable automated tests.

## Later
Held-note/polyphonic lifecycle, simultaneous patch performance, independent sequencing, key/velocity zones, patch-level automation, effects, preset import/library management and hardware compatibility matrix.
