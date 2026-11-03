# FM1-Lab

**FM1-Lab is our own instrument-first FM sound-design environment.** Its interface and workflow are the product; firmware runtimes and emulators are supporting engineering tools.

## Site layout

- **FM1-Lab** — the main application: one Instrument contains four independent FM patches, each with its own sound-design workspace.
- **[Felucca runtime](https://albinoiguana.github.io/FM1-Lab/felucca/)** — Felucca's browser-native firmware build, retained as the real firmware/DSP reference while we develop the custom application bridge.
- **[Compatibility bench](https://albinoiguana.github.io/FM1-Lab/compatibility/)** — the upstream FM-1 emulator, used for firmware compatibility and regression work. It is not the FM1-Lab product UI.
- **[Test console](https://albinoiguana.github.io/FM1-Lab/testing.html)** — firmware test utilities.
- **[Source repository](https://github.com/albinoIguana/FM1-Lab)**.

## Engine status

The current operator editor and local Web Audio synth are a **design prototype**. They are useful for exercising the four-patch Instrument model and interface interactions, but they are not bit-accurate stock FM-1 or Felucca audio. The UI labels this distinction explicitly.

Felucca is the intended firmware/DSP foundation. Its browser host exposes `fh_*` functions for boot, audio rendering, controls, screen/LED state and flash persistence. The immediate engineering task is a clean, testable adapter between FM1-Lab's Instrument/patch model and Felucca's actual FM6 patch state. We must prove the mapping and round-trip behavior before calling the local controls engine-connected.

## Instrument model

- An **Instrument** is one preset containing exactly four independent FM patches.
- Selecting an Instrument changes the active four-patch set.
- Selecting a patch opens its own sound-design workspace.
- FM algorithm topology, carrier/modulator roles, feedback and operator envelopes must ultimately come from Felucca's real FM6 implementation, not a parallel approximation.
- The sampler is outside the current scope; FM synthesis and sound design come first.

## Supporting source pins

- Felucca: `743551712c36b3984392eea1dbe13a309907e17a`.
- Upstream FM-1 emulator submodule: `9de1ef053a3c4bf56f16f05b5f0de193f4608a1d`.

The emulator remains a compatibility and regression bench. We are not building a firmware picker or presenting its UI as FM1-Lab.

## Build and tests

The Pages workflow runs the FM1-Lab tests, Felucca's FM6/browser-host regression suite, and upstream emulator tests. It builds the emulator separately under `/compatibility/`, publishes the Felucca browser runtime under `/felucca/`, and serves FM1-Lab's own interface at the site root.

Run `npm test` for the application model tests. Building Felucca's browser runtime requires Python 3 with Pillow/fontTools and Emscripten; the upstream emulator also requires its Rust/WebAssembly toolchain.

## Licensing

Felucca is GPL-3.0-only. Its engine library is MIT and its FM-1 board layer is MPL-2.0. The separate upstream FM-1 emulator is MIT-licensed. Preserve source attribution and corresponding source obligations when distributing builds.
