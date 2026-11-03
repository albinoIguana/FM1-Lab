# FM1-Lab

Independent FM synthesis design environment for the M-VAVE FM-1 ecosystem.

## Live app

[Open FM1-Lab](https://albinoiguana.github.io/FM1-Lab/) · [GitHub repository](https://github.com/albinoIguana/FM1-Lab)

The `main` branch automatically deploys to GitHub Pages through `.github/workflows/deploy-pages.yml`.

## Run locally
Clone the repository, then run `python3 -m http.server 8000` from its folder and open http://localhost:8000. No build tool or package install is required. Run `npm test` to execute the zero-dependency model and DSP tests.

## Current build
- Instrument home screen with four factory Instrument presets.
- Each Instrument contains exactly four independent six-operator FM patches.
- Preset selector changes the active Instrument; Knob 1–4 style controls select the four FM patches.
- Each patch opens the existing six-operator routing map, operator editor, macro controls and play-test engine.
- Local Instrument library persistence, legacy single-patch migration and complete Instrument JSON export.
- Three local DSP topologies: six-stage cascade, three paired carriers, and split/converging tree.
- Six sine oscillators per played note, audio-rate frequency modulation, summed carriers, per-operator ADSR and delayed feedback loops.
- Lazy-loaded official firmware emulator iframe using its documented `?embed=1` mode.
- Public GitHub Pages deployment with automatic deploy on every `main` push.

## Upstream emulator

The MIT-licensed upstream FM-1 emulator is pinned as a Git submodule at vendor/fm1-emu. GitHub Pages checks out the submodule and serves it under the FM1-Lab site path, keeping the emulator and host on the same origin. Update the pinned commit deliberately and retain the upstream LICENSE.

## Firmware test console
The [FM-1 Test Console](testing.html) embeds the full upstream emulator interface (not just its instrument-only embed). It provides the upstream firmware catalog, local `.fwsc` loading, emulated front-panel input, supported MIDI input, and browser flash save/backup controls. It is interactive firmware validation, not yet an automated regression runner: the upstream boot-script and flash-overlay try-out files are restricted to the emulator's own origin, and its internal state is not exposed to FM1-Lab.

## Data model
An **Instrument** is the saved preset and contains four **FM patches**. Each FM patch contains its own topology, macros, and six operator parameter sets. The four-patch model is now the home-screen/navigation structure; simultaneous layering, independent sequencing, and performance routing are future work.

## Engine boundaries
The Web Audio engine is an independent six-operator FM implementation, not a bit-accurate emulation of M-VAVE stock firmware. Its three routing algorithms are FM1-Lab design choices, not claims about the stock instrument's internal algorithms. The official emulator runs separately in an iframe and remains the authority for stock-firmware behavior.

The upstream iframe is directly interactive. Same-origin restrictions prevent FM1-Lab from reading its internal state or using the emulator's same-origin boot-script mechanism from this different origin. Arbitrary live parameter writes and MIDI output are not claimed.

See [accelerated roadmap](docs/roadmap.md) and [emulator capability notes](docs/emulator-capabilities.md).
