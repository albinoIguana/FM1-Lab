# Emulator capability investigation

Research snapshot: 2026-11-03. Re-verify against upstream source before implementation.

References:
- https://joakimeriksson.github.io/fm1-emu/
- https://github.com/joakimeriksson/fm1-emu

The upstream project executes FM-1 firmware in WebAssembly and models display, keys, buttons, encoders, master control, audio output and MIDI input. Its browser UI documents firmware selection/start, user-supplied stock firmware package loading, DX7 SysEx bank loading, save/restore and MIDI input. The published UI explicitly notes no MIDI output.

Its try-out URL mechanism accepts same-origin flash and time-stamped input scripts (MIDI bytes, key/button events, encoder clicks). This is not a documented live parameter RPC API.

| Capability | FM1-Lab status |
|---|---|
| Launch official emulator separately | Available |
| Arbitrary synth parameter write | Not documented; unavailable until proven |
| Emulator MIDI OUT | Not supported by published page |
| Boot-time input script | Upstream mechanism documented; adapter not implemented |
| Local independent FM synthesis | Implemented, separate engine |
| Bit-accurate stock firmware emulation | Not implemented |

Next: inspect upstream web/main.js, web/tryout.js, worklet/WASM exports, native fm1-live interfaces, and license/distribution implications. Do not bundle firmware or upstream binaries without review.