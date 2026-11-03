# Emulator capability investigation

Research snapshot: 2026-11-03. Findings below are based on the upstream `web/main.js`, `web/tryout.js`, `web/index.html` and README.

## Confirmed integration surface
- `?embed=1`: the upstream page enters an embed-only UI mode (its own stylesheet hides the surrounding site chrome). FM1-Lab now embeds this page in an iframe.
- The emulator runs firmware unchanged in WebAssembly and has a real AudioWorklet audio graph. Its page owns its AudioContext, WASM instance, firmware boot and machine state.
- Parent notification: `tell()` posts `{source:"fm1-emu", type:"tryout", state, text}` with `targetOrigin=location.origin`. This is only try-out lifecycle status. Because the iframe is hosted on `joakimeriksson.github.io`, a parent on another origin cannot receive that origin-restricted message.
- Try-out URL parameters: `?flash=PATH` and `?script=PATH`. `web/tryout.js` explicitly restricts these resources to the emulator page's own origin, over HTTP(S), with redirects rejected.
- Script syntax is fm1-live's `FM1_BENCH_KEYS`: time-stamped MIDI bytes into the emulated MIDI IN jack, key/button press/release IDs, encoder clicks and pot events. It runs from boot; it is not a live host RPC.
- Emulator input can be driven inside the iframe directly by its own UI, keyboard, mouse/touch and Web MIDI input where the browser supports it.

## Boundaries
| Capability | Status |
|---|---|
| Embed official emulator UI | Supported and integrated in FM1-Lab |
| Interact directly with emulator canvas | Supported inside iframe |
| Read emulator internal state from FM1-Lab | Not exposed cross-origin |
| Receive upstream try-out postMessage in cross-origin parent | Blocked by target-origin restriction |
| Supply try-out script from FM1-Lab origin | Rejected by same-origin validation |
| Arbitrary live parameter write API | Not documented |
| Emulator MIDI OUT | Not implemented by published page |
| Local independent FM synthesis | Implemented separately in FM1-Lab |

## Potential deeper integration
A robust scripted integration would require a same-origin companion page or upstream change (for example, a reviewed host API / configurable postMessage target origin and explicit message schema). Do not use permissive wildcard messaging or scrape internal WASM exports as if they were stable APIs. Any upstream contribution should be reviewed for license, security and compatibility.

References:
- https://github.com/joakimeriksson/fm1-emu/blob/master/web/main.js
- https://github.com/joakimeriksson/fm1-emu/blob/master/web/tryout.js
- https://github.com/joakimeriksson/fm1-emu/blob/master/web/index.html
- https://github.com/joakimeriksson/fm1-emu
