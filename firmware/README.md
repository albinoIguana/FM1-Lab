# FM1-Lab native firmware

This is the device firmware project. The browser simulator in the repository root is a separate development aid.

## Build

Requires the JieLi Linux toolchain and AC79 SDK. CI installs pinned versions through the shared FM1 builder.

```sh
git submodule update --init --recursive firmware/base firmware/upstream/felucca
cd firmware
python3 base/tools/build.py app
python3 base/tools/build.py app --host
```

The build uses the pinned `fm1-base` platform/BSP and the pinned Felucca FM6 core and patch generator. Do not update either submodule without rerunning the device build, host tests, storage checks and size checks.

## Current milestone

Native platform bring-up and FM6 core integration. Four factory FM6 patches are loaded into one Instrument, with a shared eight-voice budget and topology-aware BRIGHTNESS, BODY, MOTION and DIRT macros. Instrument bank persistence, the two-lane sequencer and the final home/editor UI are the next milestones.

No physical hardware audio test has been claimed.
