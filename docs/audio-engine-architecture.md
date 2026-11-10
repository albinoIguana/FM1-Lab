# Native FM1 audio engine

## Active device target

The active device target is the native **fm1-base + Felucca-BSP** build, not the prebuilt M-VAVE SDK archive. The latter is retained as a browser/reference submodule; its private synth symbols are not exported for application-level reuse.

The device app compiles the pinned Felucca FM6 core directly through `app/dsp/fm6.c`. The Felucca core is Apache-2.0; the FM1-Lab four-patch wrapper is GPL-3.0-only. The generated FM6 lookup tables and factory voices come from the pinned Felucca submodule via the generators declared in `app/fm1.toml`.

## Current live audio path

1. `app_init()` calls `fm6_init()`, then loads the saved panel/settings state.
2. Panel key presses and incoming USB/BLE/TRS MIDI note messages call `fm6_note_on/off` from the audio owner.
3. Main-loop controls publish compact events; `app_render()` drains them in the audio interrupt before calling `fm6_render()`.
4. `fm6_render()` produces stereo 24-bit audio at the platform-provided master gain.

The FM6 wrapper contains eight patch slots (two four-patch sequencer Instrument banks) and **eight independent eight-voice allocators (64 total voice slots)**. Each patch has a fixed pool of eight voices; allocation and voice stealing are scoped to that patch, so a busy layer cannot steal another layer's notes. A normal single-patch performance therefore renders only that patch's sounding notes, not four layers by default. This is a structural polyphony guarantee, not a claim about available CPU headroom.

The engine supports per-patch enable and level, non-destructive BRIGHTNESS/BODY/MOTION/EDGE macros, algorithm-aware carrier identification, note masks, pitch bend and tuned notes. The active app now loads the 32-Instrument / four-bank store described in the README, with audio-owner Instrument swaps.

## FM1 headroom research and what it proves

The multitrack headroom question has documented answers; it is not an open-ended guess:

- M-VAVE's official V16 release notes (2026-10-21) describe eight simultaneous timbral parts and 20 synthesis voices dynamically shared across those parts. The official open-source V14 application exposes eight track rows, NORMAL/PLAY/OFF roles, per-track MIDI input channels, and a native DX7 voice library.
- FM-Stud1o, a Felucca 1.6.1-derived FM-1 firmware, documents eight synth tracks, up to eight voices per track, second-core distribution balanced by measured per-track render cost, and voice shedding when the audio deadline is threatened. Its serial `prof` diagnostic reports average and worst cost per 2.9 ms audio block for events, each part render, each part FX chain, buses, master and output.
- These are concrete precedents for multitrack sequencing and for running multiple FM parts on the AC79 dual-core device. They do not, by themselves, certify this wrapper's 32-voice workload: FM-Stud1o's measured scheduler, second-core split and voice-shedding policy are not yet implemented in this small fm1-base app.

The current engine allocates 8 voices per patch (64 slots across both sequencer Instrument banks) and renders every live voice. The host stress test fills all eight pools at once, verifies each remains at eight after overfilling another layer, and renders one second in 128-frame test chunks. The FM6 core advances in 32-frame CTL quanta; the pinned platform calls the app with 256-frame audio blocks. The test validates integration and output bounds only; host execution time is not a proxy for device CPU headroom.

### Voice-level dual-core rendering

The device build now enables fm1-base's optional CPU2 worker with `cpu2 = true` and `cpu2_fallback = true`. Every 32-frame FM6 quantum, the renderer packs the currently sounding voice indices into a compact list and divides that list in half. This is deliberately a **voice** split, not a fixed patch-index split: a chord played on one patch is distributed across both cores. Core 0 renders its half while CPU2 renders the other half into a separate patch accumulator; core 0 joins the accumulators before the existing sequencer, FX buses and master chain continue.

The FM6 core's `FELUCCA_CPU2` mode is enabled when the device build includes CPU2. It changes the two internal operator buses from shared static storage to per-call stack storage, which is required to avoid a race between concurrent voice renders. The two cores also have separate output scratch and bus buffers in `.pool`. Before handing over the second half, core 0 snapshots those voices; if CPU2 is late or fails partway through, it restores the snapshots and renders that half itself. This keeps the single-core fallback musically correct.

The host FM6 test exercises serial rendering, a successful split, and a simulated partial-worker failure; all three must produce bit-identical output for a full eight-voice chord on one patch. fm1-base defaults the CPU2 stack to 2,816 bytes; Shipwreck explicitly requests 4,096 bytes (4 KiB) for the FM6 worker's call depth and per-call operator buses. The Home diagnostic shows CPU2's stack high-water use alongside the C2 indicator. fm1-base raises the device's VDC14 and SYSVDD rails to M-VAVE's levels before app_init starts CPU2. The second core and core 0 still fetch DSP instructions from shared XIP flash. A measured hot-path move into SRAM may further reduce cache/flash contention, but it is a separate qualification step: community device tests report 6–35% lower worst-block peaks after SRAM relocation, while another firmware's experimental fast-RAM mode was reported to fault under dual-core load. We do not relocate code speculatively in this change.

Device qualification still needs to verify actual CPU reduction, the CPU2 stack high-water mark, XRUNs and overload-shed deltas with dense chords and all FX. The second core is an optimization, not a new minimum requirement: a unit that cannot start or keep CPU2 healthy continues on core 0.

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

The app now exposes read-only `app_audio_cpu_pct()`, `app_audio_xruns()` and `app_audio_overload_shed_count()` accessors, backed directly by the pinned platform counters and FM6's overload counter. The host simulator's `app_expect` interface also exposes `cpu`, `xruns` and `shed` for automated checks. These are diagnostic interfaces only: no screen layout or render-path logging was added. On hardware, call/sample these accessors while enabling all four patches, dense two-lane sequences, the arpeggiator and all effects; record peak CPU percentage and xrun/shed counter deltas. The platform CPU value is smoothed over roughly 16 blocks, so also inspect xrun deltas during sustained stress. Host timing is not a device render-budget measurement.

## Active sequencer, arpeggiator and FX path

The active FM6 renderer now owns the timing and note generation; these are not calls into the older SDK adapter in `firmware/fm1lab_sequencer.c`.

- **Two-lane sequencer:** two independent 64-step lanes, each with patch assignment, note, velocity, enable and gate. The native UI starts with a 16-step pattern and the SELECT encoder navigates the 64-step address space. Both lanes dispatch into the same FM6 allocator used by keyboard and MIDI notes. Lane-specific source tags prevent a lane's note-off from releasing a same-pitch note played by the other lane or by the arpeggiator.
- **Arpeggiator:** captures up to 16 held keyboard/MIDI notes, sorts them by pitch, preserves their velocities, and generates UP, DOWN or UP/DOWN note events on the transport clock. Generated notes enter the FM6 allocator through an internal path so they are not re-captured as new key input.
- **Clock:** the transport phase accumulator generates 16th-note events at 30–300 BPM using the pinned fm1-base device rate of 44.1 kHz (`base/device/main_fm1.c`, `FS 44100u`). At 120 BPM the first tick occurs on sample 5513. Sequencer gates and arp notes are advanced inside `fm6_render()`, not by the main UI loop. 
- **FX routing:** each of the four FM6 patches has independent 0–100 sends to shared CHORUS, DELAY and REVERB buses. The three buses are processed in parallel after patch mixing. Chorus uses offset left/right taps for stereo width; delay and reverb are shared mono returns. Each has a wet-return control and time/feedback parameters; delay feedback and reverb/chorus regeneration are separate. The active UI exposes one effect per page, bypass, wet amount, time/feedback and the selected patch's send level.
- **DSP buffers:** the effects use fixed-size integer ring/comb buffers and no allocation or flash I/O in the render path. The delay, chorus and three reverb comb buffers occupy 96 KiB in the platform `.pool` region.

Sequencer patterns, both lane assignments, tempo, arp mode, FX enable/mix/time/feedback and all per-patch FX sends are now included in the CRC-protected settings record. SAVE snapshots the live FM6 performance state; boot restores it stopped. Existing compact settings records have migration paths. Runtime transport phase, held notes and delay/reverb history are intentionally not persisted. The two-lane UI is an initial functional editor, not yet the final sequencing workflow (no swing, tie editing or MIDI clock sync).

## Settings-record integrity tests

The active app and host tests now share the same settings-record codec in `app/fm1lab_settings_record.h`. The native suite checks the current CRC envelope against single-bit corruption at every byte, rejects correctly re-sealed but out-of-range fields, validates both legacy V1 and V2 records, and verifies migration preserves the new performance defaults while importing supported legacy fields. Invalid legacy records are rejected without partially modifying the destination. These are codec-level tests; the physical FM1 flash A/B write/recovery path remains a separate hardware validation item.

## Remaining validation

- Host tests exercise sequencer note dispatch, arp scheduling, each FX processor and per-patch send isolation; the native build and emulator must pass on the same final commit before calling this integration build-verified.
- No physical FM1 playback, actual device sample-rate confirmation, 32-voice-plus-FX CPU headroom, xrun profiling, flash/recovery or long-duration MIDI test has been completed.
- The final visual design remains a work in progress; these pages are functional controls, not the completed global design system.

## Validation

The FM6 host test compiles the same `app/dsp/fm6.c` and pinned Felucca core used by the active device build. It now exercises a one-second combined stress run with all 32 voice slots occupied, both sequencer lanes active, 16 held arp notes and all three FX buses at maximum wet/send, checking voice bounds and Q23 output bounds. It also tests the platform overload callback: fm1-base calls `app_overload()` when a measured render block exceeds 90% of its 5.8 ms deadline; FM1-Lab then fades the globally oldest voice over 32 samples and frees that slot without changing another patch's allocator. This is a safety mechanism, not proof that the workload meets the deadline. The native device workflow runs the host tests and builds the firmware with fm1-base/Felucca-BSP. Host timing is not used as a device CPU estimate.
