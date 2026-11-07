# FM1-Lab

**FM1-Lab is an independent four-patch Instrument design environment for the M-VAVE FM1.** The firmware foundation is M-VAVE's own open FM1 source and SDK. The Instrument model, macro system, editor and workflow are our own.

## Product model

- One **Instrument** contains exactly four independent six-operator FM patches.
- Each patch uses M-VAVE's expanded 156-byte DX7 voice format.
- The Instrument selector changes the four-patch bank; patch selection chooses one of its four voices.
- Four performance macros are implemented as non-destructive transformations of the saved patch: **Brightness**, **Body**, **Motion** and **Edge**.
- Brightness and Body act on carrier operators; Edge acts on modulators and algorithm feedback. Carrier roles come from M-VAVE's actual DX7 algorithm routing table.
- Operator editing changes the underlying patch; macro positions are applied as a separate live layer.

## Official firmware foundation

The pinned submodule vendor/m-vave-fm1 points to [M-VAVE's official FM1 open-source repository](https://github.com/m-vave/fm1_open). It contains the device application, board interfaces, FM1 SDK, official synthesis code, and browser simulator. The simulator is used as the sound engine for the browser workshop and for rapid verification of patch behavior.

## Current implementation

- Four-patch Instrument data model with import/export.
- M-VAVE's official DX7 factory voices as the starting library.
- Native 156-byte voice editing: algorithm, feedback, operator mode/coarse/fine, output, and four-stage envelopes.
- Algorithm-aware macro transforms.
- Browser audio powered by the official M-VAVE simulator and its own WASM audio worklet.

The browser workshop is a development and audition surface. The active native fm1-base app is a separate Felucca FM6 implementation; native device builds are checked in CI, but physical FM1 playback and performance validation remain outstanding.

## Native firmware core

The active `app/app.c` build connects the four-patch Instrument model to the Felucca FM6 voice engine. ALGO changes banks on the Home screen; PRESET selects among the eight Instruments in that bank; SELECT chooses one of the four patches inside the active Instrument. Each Instrument stores four packed DX7 voices plus per-patch macro, enable and level state. The four bank records use the platform's existing pooled, CRC-protected A/B object store (four 3,840-byte objects in the 12-sector pool), not the upstream FM1 synth engine or reserved factory/system sectors. The bank payload has its own version and CRC16. The on-device label shows the current bank and Instrument index.

The bank format uses 11-character persisted Instrument names to fit all 32 voices and their per-patch state inside the platform's 3,840-byte object limit. Existing three-slot Instrument objects are migrated into the first three Instrument positions when the bank format is initialized. Instrument loading is handed to the audio task through the app event queue so the UI thread does not write live FM voice state.

The active app also has dedicated operator-envelope and two-page LFO editors. The envelope page edits each DX7 operator's four rates and four levels; the LFO pages follow the official FM1 voice map for waveform, speed, pitch/amp depth, delay, pitch sensitivity, LFO key sync, and oscillator sync. These edits are queued to the audio task as voice snapshots rather than writing DSP state from the UI thread.

The active FM6 audio path includes a sample-clocked two-lane sequencer, held-note arpeggiator (UP / DOWN / UP-DOWN), four stereo FX buses (CHORUS / PHASER / DELAY / REVERB), per-patch send levels, Instrument-wide Amp/Drive and parametric EQ, a stereo-linked master compressor, and a final peak limiter. These are implemented in `app/dsp/fm6.c`, not the older SDK adapter. Sequencer patterns, tempo, lane assignments, arp mode, FX parameters/sends, and compressor settings are captured in the CRC-protected performance settings record and restored at boot. Instrument voices and master insert state are saved together in the versioned Instrument library. Native host regression tests cover the two sequencer lanes, generated arp output, stereo FX, per-patch send isolation, master inserts, linked compression, output bounds, and render chunk determinism. The device build and browser emulator build pass in CI; physical FM1 playback, flash migration/recovery, and real-time CPU/xrun validation remain outstanding.

## Build and validation

GitHub Actions checks the Instrument and macro models, applies the M-VAVE integration patch to the pinned source, builds the official browser simulator with WASI SDK 27, and verifies the FM1-Lab API exports.

A device firmware build requires the matching JieLi Windows toolchain 2.5.3. The official source kit requires the exact 2.5.3 toolchain and validates its binaries against the SDK manifest. See the official M-VAVE README for installation and packaging requirements.

## Licensing

The M-VAVE application is GPLv3 with its FM1 SDK linking exception. The proprietary engine is supplied as a binary SDK with an explicit FM1-only linking and redistribution grant. Preserve M-VAVE's license, third-party notices and SDK terms when distributing any build. See the license and third-party notices in vendor/m-vave-fm1.
