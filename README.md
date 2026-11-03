# FM1-Lab

**FM1-Lab is our own instrument-first FM sound-design environment.** Its interface and workflow are the product; Felucca's firmware engine is the synthesis foundation, and the upstream emulator remains a supporting compatibility tool.

## Instrument model

- An **Instrument** is one preset containing exactly four independent FM patches.
- The first two Instrument banks are backed by Felucca's eight actual FM6 factory voices: slots 1–4 and 5–8.
- Selecting an Instrument loads its four voices into Felucca's four native synth parts.
- Selecting a patch selects the corresponding native part for playback and editing.
- Instrument edits are saved locally, including the complete 155-byte native FM6 voice data for each patch.

## Native engine integration

FM1-Lab runs Felucca's own browser-native WebAssembly firmware inside its AudioWorklet. Felucca owns the audio rendering, voice allocation, FM6 algorithms, envelopes, modulation and effects; FM1-Lab does not substitute a parallel Web Audio FM engine.

A narrow patch overlay against the pinned Felucca host exposes:
- Four native FM6 tracks.
- Native 155-byte patch read/write and a dedicated scratch buffer.
- Felucca's eight factory FM6 voices.
- The actual carrier masks, modulation routing masks and feedback operator for all 32 algorithms.

The custom UI maps its operator ratio, output level, four envelope controls, algorithm, feedback and voice name into the native patch bytes. Algorithm diagrams and carrier/modulator roles come from Felucca's own routing tables.

The current editor deliberately exposes only the subset of the 155-byte voice format that has a direct UI control. The remaining native voice fields are preserved when these controls are changed. Full DX-style four-stage envelope editing, modulation matrix, LFO and FX pages are subsequent work.

## Site layout

- **FM1-Lab** — the main application and custom Instrument workflow.
- **[Felucca runtime](https://albinoiguana.github.io/FM1-Lab/felucca/)** — Felucca's own browser-native firmware reference.
- **[Compatibility bench](https://albinoiguana.github.io/FM1-Lab/compatibility/)** — upstream FM-1 emulator for firmware compatibility and regression work. It is not the FM1-Lab product UI.
- **[Test console](https://albinoiguana.github.io/FM1-Lab/testing.html)** — firmware test utilities.
- **[Source repository](https://github.com/albinoIguana/FM1-Lab)**.

## Supporting source pins

- Felucca: `743551712c36b3984392eea1dbe13a309907e17a`.
- Upstream FM-1 emulator submodule: `9de1ef053a3c4bf56f16f05b5f0de193f4608a1d`.

## Build and tests

The Pages workflow runs the FM1-Lab unit tests, builds Felucca's browser runtime with the FM1-Lab host API overlay, boots the resulting WASM in a smoke test, checks all 32 native routing graphs, loads all eight factory voices, verifies native patch read/write isolation, and runs Felucca's broader regression suite. It also runs the upstream emulator's tests and compatibility checks.

Run `npm test` for the application and native patch mapping tests. Building Felucca's browser runtime requires Python 3 with Pillow/fontTools and Emscripten; the upstream emulator also requires its Rust/WebAssembly toolchain.

## Licensing

Felucca is GPL-3.0-only. Its engine library is MIT and its FM-1 board layer is MPL-2.0. The separate upstream FM-1 emulator is MIT-licensed. Preserve source attribution and corresponding source obligations when distributing builds.
