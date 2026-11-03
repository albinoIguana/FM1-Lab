# FM-1 emulator integration

Research snapshot: 2026-11-03. Upstream source is pinned as a recursive Git submodule at `vendor/fm1-emu` (MIT license), commit `9de1ef053a3c4bf56f16f05b5f0de193f4608a1d`.

## Primary runtime
- The upstream `tools/build_site.sh` builds the actual emulator site into the GitHub Pages artifact root.
- The root page is the upstream FM-1 interface itself, not an iframe or a reimplementation.
- The emulator runs the FM-1 CPU, display, controls and audio in WebAssembly; community firmware packages run unchanged.
- The upstream page's own firmware picker, Start button, keyboard/touch input, MIDI input, flash persistence and backup UI remain intact.
- The experimental FM1-Lab Instrument designer is staged separately at `/designer/`.

## Scripted test surface
- The root page accepts the upstream documented `?fw=felucca&script=scripts/felucca-midi-smoke.fm1` try-out URL.
- The Felucca MIDI smoke script sends note-on and note-off events on channel 1.
- The upstream emulator reports documented try-out lifecycle messages (`loading`, `booting`, `playing`, `start`, `error`). Parent pages validate both origin and iframe source before displaying them.
- The upstream CI smoke script boots the firmware, checks rendered LCD pixels and verifies audible output from MIDI input.
- The upstream persistence test verifies a flash save/restore round trip.

## Boundaries
| Capability | Status |
|---|---|
| Emulate FM-1 CPU, screen, controls and audio | Implemented upstream in WebAssembly |
| Run supported community firmware unchanged | Implemented upstream |
| Use original firmware picker, Start, keyboard/touch and MIDI controls | Available on primary root page |
| Load scripted MIDI/key/encoder events at boot | Supported by upstream try-out API |
| Verify Felucca boot, LCD and audible MIDI response | Implemented in CI |
| Verify flash persistence round trip | Implemented in CI |
| Compare recorded audio against a golden reference across builds | Not implemented |
| Read arbitrary internal WASM state from a host page | No stable public API |
| Arbitrary live parameter writes from FM1-Lab | No documented host API |
| Former FM1-Lab local DSP | Independent prototype, not stock firmware emulation |

## Development sequence
1. Validate the primary root experience on desktop and iOS Safari.
2. Add scenario orchestration and retained test artifacts.
3. Investigate a stable host API with upstream rather than scraping internal WASM exports.
4. Map the four-patch Instrument abstraction only to real firmware parameters and workflows.

Retain upstream attribution and license. Update the pinned commit deliberately, then rerun the complete emulator build and firmware smoke matrix.
