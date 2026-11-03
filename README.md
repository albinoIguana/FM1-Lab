# FM1-Lab

Independent FM synthesis design environment for the M-VAVE FM-1 ecosystem.

## Run locally
`python3 -m http.server 8000` then open http://localhost:8000. No build tool or package install is required.

## Current build
- Responsive workbench, six-operator routing map and editable operator parameters.
- Three real DSP topologies: six-stage cascade, three paired voices, and split/converging tree.
- Six sine oscillators per note, audio-rate frequency modulation, summed carriers, per-operator ADSR and delayed feedback loops.
- Brightness, body, motion and feedback-edge macros.
- Local patch persistence, mutation, JSON export and play-test keyboard.
- Lazy-loaded official firmware emulator iframe using its documented `?embed=1` mode.

## Engine boundaries
The Web Audio engine is an independent six-operator FM implementation, not a bit-accurate emulation of M-VAVE stock firmware. Its three routing algorithms are FM1-Lab design choices, not claims about the stock instrument's internal algorithms. The official emulator runs separately in an iframe and remains the authority for stock-firmware behavior.

The upstream iframe is directly interactive. Same-origin restrictions prevent FM1-Lab from reading its internal state or using the emulator's same-origin boot-script mechanism from this different origin. Arbitrary live parameter writes and MIDI output are not claimed.

See [accelerated roadmap](docs/roadmap.md) and [emulator capability notes](docs/emulator-capabilities.md).
