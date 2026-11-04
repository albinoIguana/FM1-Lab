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

FM1-Lab does not depend on Felucca. Its code, tests and build no longer include Felucca.

## Current implementation

- Four-patch Instrument data model with import/export.
- M-VAVE's official DX7 factory voices as the starting library.
- Native 156-byte voice editing: algorithm, feedback, operator mode/coarse/fine, output, and four-stage envelopes.
- Algorithm-aware macro transforms.
- Browser audio powered by the official M-VAVE simulator and its own WASM audio worklet.

The browser workshop is a development and audition surface. The corresponding custom Instrument navigation and macro controls still need to be integrated into the FM1's on-device application and verified with the device toolchain before this can be called flashable firmware.

## Build and validation

GitHub Actions checks the Instrument and macro models, applies the M-VAVE integration patch to the pinned source, builds the official browser simulator with WASI SDK 27, and verifies the FM1-Lab API exports.

A device firmware build requires the matching JieLi Windows toolchain 2.5.3. See the official M-VAVE README for installation and packaging requirements.

## Licensing

The M-VAVE application is GPLv3 with its FM1 SDK linking exception. The proprietary engine is supplied as a binary SDK with an explicit FM1-only linking and redistribution grant. Preserve M-VAVE's license, third-party notices and SDK terms when distributing any build. See the license and third-party notices in vendor/m-vave-fm1.
