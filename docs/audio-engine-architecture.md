# Native FM1 audio engine

## Active device target

The active device target is the native **fm1-base + Felucca-BSP** build, not the prebuilt M-VAVE SDK archive. The latter is retained as a browser/reference submodule; its private synth symbols are not exported for application-level reuse.

The device app compiles the pinned Felucca FM6 core directly through `app/dsp/fm6.c`. The Felucca core is Apache-2.0; the FM1-Lab four-patch wrapper is GPL-3.0-only. The generated FM6 lookup tables and factory voices come from the pinned Felucca submodule via the generators declared in `app/fm1.toml`.

## Current live audio path

1. `app_init()` calls `fm6_init()`, then loads the saved panel/settings state.
2. Panel key presses and incoming USB/BLE/TRS MIDI note messages call `fm6_note_on/off` from the audio owner.
3. Main-loop controls publish compact events; `app_render()` drains them in the audio interrupt before calling `fm6_render()`.
4. `fm6_render()` produces stereo 24-bit audio at the platform-provided master gain.

The FM6 wrapper contains four patch slots and **four independent eight-voice allocators (32 total voice slots)**. Each patch has a fixed pool of eight voices; allocation and voice stealing are scoped to that patch, so a busy layer cannot steal another layer's notes. With all four patches enabled, each patch can sustain up to eight simultaneous notes before its own voice stealing. This is a structural polyphony guarantee, not a claim about available CPU headroom.

The engine supports per-patch enable and level, non-destructive BRIGHTNESS/BODY/MOTION/EDGE macros, algorithm-aware carrier identification, note masks, pitch bend and tuned notes. The current fm1-base bring-up app loads the first four Felucca factory patches; the full named Instrument library and user-patch persistence are not yet wired into this app.

## FM1 headroom research and what it proves

The multitrack headroom question has documented answers; it is not an open-ended guess:

- M-VAVE's official V16 release notes (2026-10-21) describe eight simultaneous timbral parts and 20 synthesis voices dynamically shared across those parts. The official open-source V14 application exposes eight track rows, NORMAL/PLAY/OFF roles, per-track MIDI input channels, and a native DX7 voice library.
- FM-Stud1o, a Felucca 1.6.1-derived FM-1 firmware, documents eight synth tracks, up to eight voices per track, second-core distribution balanced by measured per-track render cost, and voice shedding when the audio deadline is threatened. Its serial `prof` diagnostic reports average and worst cost per 2.9 ms audio block for events, each part render, each part FX chain, buses, master and output.
- These are concrete precedents for multitrack sequencing and for running multiple FM parts on the AC79 dual-core device. They do not, by themselves, certify this wrapper's 32-voice workload: FM-Stud1o's measured scheduler, second-core split and voice-shedding policy are not yet implemented in this small fm1-base app.

The current four-patch engine allocates 8 voices per patch (32 slots total) and renders every live voice. That is a deliberate independent-allocation model, not a proven real-time budget. The new host stress test fills all four pools at once, verifies each remains at eight after overfilling another layer, and renders a block. It validates allocator correctness and basic output only; host execution time is not a proxy for device CPU headroom.

**Engineering decision:** keep the four independent patch pools, but do not market 32-voice simultaneous operation as hardware-validated. The next device-side performance step is to instrument the real audio-block cost and overload count, compare the worst-case cost against the FM1's 2.9 ms scheduling interval, and add bounded voice shedding or a measured global render budget if required. Preserve patch isolation when shedding: never steal a voice from another patch merely to satisfy one patch's local eight-voice allocator.

References:
- [M-VAVE FM-1 V16 release notes](https://forum.m-vave.com/topic/23)
- [Official M-VAVE FM1 open-source app](https://github.com/m-vave/fm1_open)
- [FM-Stud1o firmware and profiling documentation](https://github.com/MPASTEAU/FM-Stud1o-FM-1-Firmware)

## What is not complete

- The two-lane sequencer is not yet integrated into the fm1-base app. The older M-VAVE SDK sequencer adapter under `firmware/` is a separate prototype and is not part of the active device build.
- The FM6 output currently reaches the platform render output directly. Official-style per-track sends, shared FX routing, and effect DSP/pages have not yet been integrated or verified.
- The current UI is a hardware bring-up screen, not the final HOME, macro, global-settings or deep-editor design.
- No physical FM1 playback, CPU headroom for 32 voices, flash/recovery or long-duration MIDI test has been completed.

## Validation

The FM6 host test compiles the same `app/dsp/fm6.c` and pinned Felucca core used by the active device build. The native device workflow runs that test, builds the firmware with fm1-base/Felucca-BSP and creates the package. A passing host test is not a substitute for physical hardware validation.
