# Emulator capability investigation

Research snapshot: 2026-11-03. Upstream source is pinned as a Git submodule at vendor/fm1-emu (MIT license), currently commit 9de1ef053a3c4bf56f16f05b5f0de193f4608a1d.

## Integration architecture
- The upstream FM-1 emulator is served from /FM1-Lab/vendor/fm1-emu/web/ on the same GitHub Pages origin as FM1-Lab.
- The Pages deployment workflow checks out submodules so the upstream WebAssembly binary, JavaScript, firmware catalog and assets are deployed with the app.
- The iframe uses the upstream embed-only mode for the main editor and the full page for the dedicated test console.
- Same-origin hosting means documented try-out URL parameters can load test scripts from FM1-Lab's own scripts directory.
- The upstream page's documented postMessage lifecycle events (loading, booting, playing, start, error) can be received by the parent after checking both origin and iframe window.

## Scripted test surface
- The Felucca MIDI smoke scenario loads the Felucca firmware and sends a MIDI note-on / note-off sequence on channel 1.
- The emulator's playing event means the scripted input sequence has been delivered to the emulated machine. It is not proof that audio was audible or that the firmware passed a semantic assertion.
- A flash URL can load documented flash-sector overlays from the same origin, enabling repeatable save-state scenarios later.

## Boundaries
| Capability | Status |
|---|---|
| Run official upstream emulator unchanged | Supported via pinned submodule |
| Run on same origin as FM1-Lab | Supported |
| Load scripted MIDI / key / encoder events at boot | Supported via documented try-out API |
| Receive documented try-out lifecycle events | Supported; origin and iframe source are validated |
| Load a flash-sector overlay from this site | Supported by upstream format; tooling still to build |
| Read arbitrary internal WASM state | Not exposed as a stable host API |
| Assert audible output or compare audio automatically | Not implemented |
| Arbitrary live parameter writes | Not documented / not implemented |
| FM1-Lab local DSP engine | Independent implementation, not bit-accurate stock firmware |

## Next integration steps
1. Verify the locally hosted emulator on desktop and iOS Safari.
2. Add a scenario runner for firmware, script, and optional flash overlay combinations.
3. Capture audio output for deterministic audio regression comparisons where browser support allows.
4. If deeper live telemetry is needed, propose a small explicit host API upstream rather than scraping private WASM exports.

The submodule preserves upstream attribution and pins a known-good revision. Update it deliberately, review upstream changes, then run the browser and firmware smoke matrix.
