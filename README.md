# FM1-Lab

An independent FM synthesis design environment for the M-VAVE FM-1 ecosystem.

## Run locally
`python3 -m http.server 8000` then open http://localhost:8000. No build step or package install is required.

## Current speedrun build
- Geometric startup and responsive instrument workbench.
- Six-operator design/routing model with editable parameters.
- Four macro controls, patch mutation, local persistence and JSON export.
- Real-time local two-operator FM voice using Web Audio API (modulator frequency-modulates carrier frequency, with amplitude envelope).
- Play-test note and on-screen keyboard.
- Lazy-loaded iframe embedding the official emulator using its documented `?embed=1` mode.

## Engine boundaries
The local Web Audio FM voice is an independent engine, not the M-VAVE stock firmware and not a bit-accurate FM1 emulator. The current audio voice uses two operators; the six-operator editor is a broader design model. Routing diagrams are illustrative.

The upstream emulator runs firmware in WebAssembly. Its `?embed=1` mode is supported. Its try-out `?flash=` / `?script=` files must be same-origin with the emulator page. Its parent status messages are posted to the emulator page's own origin, so they do not form a cross-origin control API for FM1-Lab. Arbitrary live parameter writes and MIDI output are not claimed.

See [accelerated roadmap](docs/roadmap.md) and [emulator capability notes](docs/emulator-capabilities.md).
