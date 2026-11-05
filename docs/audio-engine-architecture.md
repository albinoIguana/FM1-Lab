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

## What is not complete

- The two-lane sequencer is not yet integrated into the fm1-base app. The older M-VAVE SDK sequencer adapter under `firmware/` is a separate prototype and is not part of the active device build.
- The FM6 output currently reaches the platform render output directly. Official-style per-track sends, shared FX routing, and effect DSP/pages have not yet been integrated or verified.
- The current UI is a hardware bring-up screen, not the final HOME, macro, global-settings or deep-editor design.
- No physical FM1 playback, CPU headroom for 32 voices, flash/recovery or long-duration MIDI test has been completed.

## Validation

The FM6 host test compiles the same `app/dsp/fm6.c` and pinned Felucca core used by the active device build. The native device workflow runs that test, builds the firmware with fm1-base/Felucca-BSP and creates the package. A passing host test is not a substitute for physical hardware validation.
