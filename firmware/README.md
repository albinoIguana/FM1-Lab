# FM1-Lab native firmware

This directory contains the device firmware. The browser simulator in the repository root is a separate development aid and is not the firmware deliverable.

## Instrument design

- 16 user-named Instrument slots; each Instrument contains exactly four six-operator FM patches.
- All four patches share one measured eight-voice allocator. Each patch can be enabled or muted independently.
- K1-K4 are BRIGHTNESS, BODY, MOTION and DIRT. All default to neutral 50; edits are topology-aware and non-destructive.
- The physical PRESET encoder changes Instrument; SELECT chooses one of its four patches; ALGO changes the selected patch's algorithm.
- Short SAVE stores the current Instrument and its sequence. Hold SAVE to rename; SELECT moves the cursor and ALGO changes the character.
- Each Instrument owns a two-lane, sixteen-step sequencer pattern. PLAY starts/stops; SEQ enters step-edit mode; SELECT chooses a step; ALGO chooses lane or tempo; touch keys enter notes.
- No per-lane timing variation is implemented; tempo and gate are shared.

## Build

Requires the JieLi Linux toolchain and AC79 SDK. CI installs pinned versions through the shared FM1 builder.

```sh
git submodule update --init --recursive firmware/base firmware/upstream/felucca
cd firmware
python3 base/tools/build.py app
python3 base/tools/build.py app --host
sh tests/run_tests.sh
```

Instrument data is stored as versioned, CRC32-checked 607-byte records in three A/B pooled objects (6/5/5 Instruments per object); each object stays below the 3,840-byte sector payload limit. The platform store provides atomic copy rotation, and the app codec rejects corrupt or mismatched chunks.

The build uses the pinned `fm1-base` platform/BSP and the pinned Felucca integer FM6 core and patch generator. Do not update either submodule without rerunning the device build, host audio regression tests, storage checks and size checks.

## Hardware and validation status

The official Felucca-compatible XIP application slot is 581,564 bytes. The first native bring-up image was 151,428 bytes; the subsequent Instrument-bank build was 155,172 bytes before the sequencer integration. These are app-image sizes, not package sizes. The final sequencer/UI build must be measured again in CI.

No physical FM1 hardware audio test has been claimed. A successful CI package is not the same as a verified on-device update or audio test.
