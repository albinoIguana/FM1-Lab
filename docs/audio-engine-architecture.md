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

The engine supports per-patch enable and level, non-destructive BRIGHTNESS/BODY/MOTION/EDGE macros, algorithm-aware carrier identification, note masks, pitch bend and tuned notes. The active app now loads the 32-Instrument / four-bank store described in the README, with audio-owner Instrument swaps.

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

## Device-side headroom telemetry

The exact pinned fm1-base platform revision used by this branch (`2d8ed748cdade582c65d9fc8d8cf17da4ddadec8`) exposes:
- `plat_cpu_pct()`: audio load as a percentage of the render budget, measured by the device audio ISR.
- `plat_xruns()`: audio deadline misses since boot.
- `plat_ticks24()`: a 24 MHz free-running timer for measuring short render spans.

The platform exposes device-reported CPU percentage and xrun counters through `plat_cpu_pct()` and `plat_xruns()`. The current native UI does not yet surface both diagnostics. For hardware validation, enable all four patches, run dense two-lane sequences with the arpeggiator and effects enabled, and record peak CPU percentage and xrun changes. The reference firmware's 2.9 ms audio-block profiling is a useful comparator; host timing is not a device render-budget measurement.

## Active sequencer, arpeggiator and FX path

The active FM6 renderer now owns the timing and note generation; these are not calls into the older SDK adapter in `firmware/fm1lab_sequencer.c`.

- **Two-lane sequencer:** two independent 64-step lanes, each with patch assignment, note, velocity, enable and gate. The native UI starts with a 16-step pattern and the SELECT encoder navigates the 64-step address space. Both lanes dispatch into the same FM6 allocator used by keyboard and MIDI notes. Lane-specific source tags prevent a lane's note-off from releasing a same-pitch note played by the other lane or by the arpeggiator.
- **Arpeggiator:** captures up to 16 held keyboard/MIDI notes, sorts them by pitch, preserves their velocities, and generates UP, DOWN or UP/DOWN note events on the transport clock. Generated notes enter the FM6 allocator through an internal path so they are not re-captured as new key input.
- **Clock:** the transport phase accumulator generates 16th-note events at 30–300 BPM using a 48 kHz sample-rate assumption. Sequencer gates and arp notes are advanced inside `fm6_render()`, not by the main UI loop. Confirm the actual device audio rate and adjust the constant if the platform build is configured differently.
- **FX routing:** each of the four FM6 patches has independent 0–100 sends to shared CHORUS, DELAY and REVERB buses. The three buses are processed in parallel after patch mixing. Each has a wet-return control and time/feedback parameters; delay feedback and reverb/chorus regeneration are separate. The active UI exposes one effect per page, bypass, wet amount, time/feedback and the selected patch's send level.
- **DSP buffers:** the effects use fixed-size integer ring/comb buffers and no allocation or flash I/O in the render path. The implementation adds roughly 47 KiB of effect delay memory before compiler alignment and other state.

Sequencer patterns, arp mode, tempo and FX parameters currently initialize to defaults at boot; they are not yet saved as user data. The two-lane UI is an initial functional editor, not yet the final sequencing workflow (no swing, tie editing, MIDI clock sync or pattern persistence).

## Remaining validation

- Host tests exercise sequencer note dispatch, arp scheduling, each FX processor and per-patch send isolation; the native build and emulator must pass on the same final commit before calling this integration build-verified.
- No physical FM1 playback, actual device sample-rate confirmation, 32-voice-plus-FX CPU headroom, xrun profiling, flash/recovery or long-duration MIDI test has been completed.
- The final visual design remains a work in progress; these pages are functional controls, not the completed global design system.

## Validation

The FM6 host test compiles the same `app/dsp/fm6.c` and pinned Felucca core used by the active device build. The native device workflow runs that test, builds the firmware with fm1-base/Felucca-BSP and creates the package. A passing host test is not a substitute for physical hardware validation.
