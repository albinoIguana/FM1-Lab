# FM1-Lab accelerated roadmap

## Sprint A — Foundation and playable workbench
- [x] Responsive UI and geometric startup.
- [x] Six-operator design model, editable parameters and routing studies.
- [x] Patch persistence, mutation and JSON export.
- [x] Real-time local two-operator FM voice using Web Audio API.
- [ ] Browser smoke tests, accessibility and Pages deployment.

## Sprint B — Emulator integration proof
- [ ] Inspect upstream WASM exports, AudioWorklet and message flow.
- [ ] Separate stable public host boundary from internal implementation details.
- [ ] Verify embedding/launch path, boot, input, audio and MIDI input.
- [ ] Document license and firmware handling.
- [ ] Expose parameter writes only if a real source-backed path is proven.

## Sprint C — FM engine v1
- [x] First playable two-operator FM voice.
- [ ] Canonical patch schema and migrations.
- [ ] Six-operator graph execution and carrier summing.
- [ ] Per-operator ADSR, note-off, velocity and polyphony.
- [ ] Bounded feedback, aliasing review and audio regression tests.

Combined speedrun exit criteria: a stable workbench that produces sound from its own FM engine, exports patches, and has an evidence-based emulator integration report. The local DSP and stock firmware emulator remain separate engines.

## Later
Algorithm-aware macros, four-layer hybrid architecture, sequencing/arpeggiation, automation, effects, presets and hardware compatibility.