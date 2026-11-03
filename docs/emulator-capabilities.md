# FM1-Lab runtime architecture

Research snapshot: 2026-11-03.

## Runtime A — generic FM-1 emulator

Upstream source is pinned as a recursive Git submodule at `vendor/fm1-emu` (MIT license), commit `9de1ef053a3c4bf56f16f05b5f0de193f4608a1d`.

- Builds the emulated FM-1 CPU, display, controls and audio to WebAssembly.
- Runs supported community firmware packages unchanged.
- Remains the compatibility bench and the firmware regression target.
- Exposes documented boot-time key/MIDI/encoder scripts for repeatable smoke scenarios.

## Runtime B — Felucca browser host

Felucca source is fetched at pinned commit `743551712c36b3984392eea1dbe13a309907e17a` during the Pages build, with its nested submodules initialized recursively. The project is GPL-3.0-only; its engine library is MIT and board layer is MPL-2.0.

- `./build.sh gen && ./build.sh web` builds Felucca's `platforms/web` runtime with Emscripten.
- The firmware application sources are compiled as a WebAssembly host build against `platforms/host/felucca_host.c`.
- `platforms/host/felucca_host.h` exports the documented host surface: `fh_boot`, `fh_render`, `fh_buttons`, `fh_keys`, `fh_enc`, `fh_master`, `fh_midi`, screen/LED accessors and flash accessors.
- The browser frontend runs the firmware in an AudioWorklet and persists its emulated flash in browser storage.
- Felucca has four independent tracks/parts and an FM6 engine (six-operator FM). This is the real starting point for our four-patch Instrument model.

## What this enables

The Felucca runtime gives FM1-Lab a source-controlled firmware build and a host API designed for custom frontends. It is materially different from embedding the generic emulator: we can evolve the Felucca source and its browser host as our own development target, while keeping the generic emulator untouched for compatibility checks.

The first integration build publishes the upstream Felucca browser UI at `/felucca/`. It does not yet connect the separate Instrument designer to Felucca's internal parameter model. The next engineering step is to extend the host/worklet message surface deliberately, then build the custom UI on top of it.

## Product constraints

- The FM1-Lab boot animation remains.
- The main workspace should be hardware-oriented and information-dense, not a generic marketing/dashboard page.
- Effects follow the Clipper-inspired interaction model: choose an effect, move through concise pages, turn the physical-style controls, hear the result immediately, and make bypass obvious.
- FM topology must be algorithm-aware: identify carriers/modulators and present useful role-based macros, with a deeper operator editor still available.
- A four-patch Instrument is a higher-level object; operator routing remains local to each patch.
- Mapping the four patches onto Felucca's four tracks is the working hypothesis, not a completed implementation. A sound-only Instrument format must not accidentally serialize an entire sequencer project.

## Boundaries

| Capability | Status |
|---|---|
| Run supported FM-1 community firmware unchanged | Generic emulator |
| Build Felucca firmware sources to browser WebAssembly | Implemented in CI |
| Felucca host control/audio/screen/LED/flash API | Exported upstream |
| Custom FM1-Lab UI controlling Felucca runtime | Not implemented yet |
| Four-patch Instrument mapped to four Felucca tracks | Proposed; needs tests |
| Algorithm-aware UI and role-based macros | Design requirement; not implemented |
| Clipper-style effect page workflow | Design requirement; not implemented |
| Old local six-operator Web Audio engine | Prototype only; not the target engine |

Keep the two runtimes distinct, retain their respective licenses, and test changes against both.
