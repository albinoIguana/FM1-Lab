# FM1-Lab

**FM1-Lab is becoming a Felucca-based FM-1 development environment.** The upstream emulator remains the compatibility bench; Felucca's own browser-native firmware runtime is the foundation for our custom instrument workflow.

## Live

- [FM-1 emulator / compatibility bench](https://albinoiguana.github.io/FM1-Lab/) — the emulated FM-1 CPU, display, controls and audio running community firmware unchanged in WebAssembly.
- [Felucca browser runtime](https://albinoiguana.github.io/FM1-Lab/felucca/) — Felucca's actual firmware source compiled to WebAssembly against its simulated FM-1 host.
- [Experimental Instrument designer](https://albinoiguana.github.io/FM1-Lab/designer/) — the earlier four-patch UI and independent six-operator DSP prototype; this is not yet connected to Felucca's sound engine.
- [Firmware test console](https://albinoiguana.github.io/FM1-Lab/testing.html).
- [FM1-Lab source](https://github.com/albinoIguana/FM1-Lab).

## Two different runtimes

### FM-1 emulator

The MIT-licensed emulator is pinned as a recursive Git submodule at `vendor/fm1-emu`. It emulates the FM-1 CPU and runs supported firmware packages unchanged. It remains our compatibility and regression bench.

### Felucca firmware runtime

The GitHub Pages build fetches Felucca at pinned commit `743551712c36b3984392eea1dbe13a309907e17a`, checks out its nested source dependencies, and builds `platforms/web` with Emscripten. This is not a firmware package loaded into the generic emulator: it is Felucca's own C firmware compiled for its browser host.

Felucca's `platforms/host/felucca_host.h` exports the `fh_*` host API: boot, render audio, send keys/buttons/encoder events and MIDI, inspect the screen and LEDs, and persist flash. The browser frontend runs the same firmware application sources against a simulated FM-1.

Felucca currently provides four independent tracks/parts and a six-operator FM engine. Its firmware already draws and tests all 32 FM6 algorithm topologies and includes dedicated operator/envelope pages. That gives us a credible starting point for the four-patch Instrument idea. The exact mapping—especially sound-only Instrument save/load versus a full sequencer project—still needs to be implemented and tested.

## Product and interaction direction

- Keep the accepted FM1-Lab boot animation.
- Make the working interface feel like a purpose-built FM-1 instrument, not a generic neon dashboard: hardware-oriented controls, clear screen hierarchy, compact parameter pages and immediate feedback.
- Use Felucca's actual engine and parameter model as the source of truth.
- Use Clipper as a workflow reference only: choose an effect, navigate its pages, turn the hardware-style controls, hear the change immediately; keep bypass and navigation obvious and advanced detail tucked away.
- Preserve Felucca's existing 32-algorithm FM6 topology graphs, carrier/modulator/feedback roles and per-operator edit pages. Extend the four-knob macro layer around those real engine semantics instead of duplicating or approximating the graph.
- Keep DSP modular. The four-patch Instrument is a higher-level container; operator routing remains inside each patch.

## Build locally

Prerequisites: Git with submodules, Python 3 with Pillow and fontTools, Emscripten, Rust, and Node.js.

```sh
git clone --recurse-submodules https://github.com/albinoIguana/FM1-Lab.git
cd FM1-Lab
git clone https://github.com/hugelton/Felucca.git vendor/felucca
git -C vendor/felucca checkout 743551712c36b3984392eea1dbe13a309907e17a
git -C vendor/felucca submodule update --init --recursive
python -m pip install Pillow fonttools
(cd vendor/felucca && ./build.sh gen && ./build.sh web)
rustup target add wasm32-unknown-unknown
vendor/fm1-emu/tools/build_site.sh _site
mkdir -p _site/felucca _site/designer _site/scripts
cp -R vendor/felucca/build/web/. _site/felucca/
cp vendor/felucca/LICENSE vendor/felucca/LICENSING.md _site/felucca/
cp -R vendor/felucca/LICENSES _site/felucca/
cp index.html _site/designer/index.html
cp -R src _site/designer/
cp testing.html _site/testing.html
cp .nojekyll _site/
cp scripts/felucca-midi-smoke.fm1 _site/scripts/
node scripts/inject-lab-link.mjs _site/index.html
python3 -m http.server 8000 --directory _site
```

Run `npm test` for FM1-Lab's model and integration checks. The browser runtime must be served over HTTP; opening its HTML file directly will not work.

## Licensing and attribution

Felucca is GPL-3.0-only. Its engine library is MIT and its FM-1 board layer is MPL-2.0; their notices are included in the deployed Felucca runtime. The separate FM-1 emulator is MIT-licensed. Keep source attribution and corresponding source available for distributed builds.
