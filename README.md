# FM1-Lab

**FM1-Lab is now emulator-first.** The main page runs the actual M-VAVE FM-1 emulator in the browser. The custom four-patch Instrument designer remains available as a secondary experiment.

## Live

- [Play the FM-1 emulator](https://albinoiguana.github.io/FM1-Lab/) — the real emulated FM-1 CPU, screen, controls and audio running community firmware unchanged in WebAssembly.
- [Experimental Instrument designer](https://albinoiguana.github.io/FM1-Lab/designer/) — the separate four-patch Instrument model and local six-operator DSP prototype.
- [Firmware smoke-test console](https://albinoiguana.github.io/FM1-Lab/testing.html) — scripted Felucca test entry point.
- [FM1-Lab source](https://github.com/albinoIguana/FM1-Lab).

The upstream emulator is MIT-licensed and pinned as a recursive Git submodule at `vendor/fm1-emu`. Its original interface is built and served at the site root, so there is no iframe between the user and the audio engine. GitHub Pages builds the WebAssembly runtime, runs upstream Rust and browser tests, smoke-tests Felucca audio/MIDI, verifies flash persistence, and deploys automatically on pushes to `main`.

## Run locally

Clone with submodules:

```sh
git clone --recurse-submodules https://github.com/albinoIguana/FM1-Lab.git
cd FM1-Lab
rustup target add wasm32-unknown-unknown
vendor/fm1-emu/tools/build_site.sh _site
mkdir -p _site/designer
cp index.html _site/designer/index.html
cp -R src _site/designer/
cp testing.html _site/testing.html
mkdir -p _site/scripts
cp scripts/felucca-midi-smoke.fm1 _site/scripts/
node scripts/inject-lab-link.mjs _site/index.html
python3 -m http.server 8000 --directory _site
```

Open http://localhost:8000. The emulator needs to be served over HTTP; opening the HTML file directly will not work. Run `npm test` for FM1-Lab's model and integration checks.

## Product direction

The upstream emulator is the source of truth for firmware behavior. It emulates the FM-1 itself—CPU, screen, keys, knobs and audio—and runs supported community firmware unchanged. Start with Felucca or another community firmware from its built-in catalog. M-VAVE stock firmware must be supplied by the user as an `.fwsc` package.

The former FM1-Lab editor is retained at `/designer/` as a prototype, not presented as an emulation of stock FM-1. Its local Web Audio engine is an independent six-operator FM implementation. Its four-patch Instrument abstraction remains a possible higher-level authoring layer, but we will only connect it to real emulator behavior through a documented, testable interface—not by pretending its parameters already control firmware.

## Current emulator integration

- Upstream source pinned at `9de1ef053a3c4bf56f16f05b5f0de193f4608a1d`.
- Upstream emulator and FM-1 SID site built from the recursive submodule.
- Emulator served directly at the root of FM1-Lab, retaining its own firmware picker, Start control, keyboard/touch controls, MIDI input, flash saves and backups.
- Same-origin try-out scripts can load scripted key/MIDI/encoder events and report lifecycle status.
- CI tests the upstream Rust/browser code, builds WebAssembly, boots Felucca, checks LCD output and audible MIDI response, and verifies flash persistence.

## Next work

1. Test the root emulator on desktop and iOS Safari, including audio start and touch interaction.
2. Add repeatable firmware scenarios and capture regression artifacts.
3. Investigate a documented host API for telemetry and live parameter control.
4. Only then connect the four-patch Instrument designer to actual emulator/firmware capabilities.
