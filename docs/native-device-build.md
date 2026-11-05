# Native FM1 device build

## Active build path

The release-target build is **fm1-base + Felucca-BSP + the pinned Felucca FM6 core**. It is not the old M-VAVE SDK/Windows packager path.

From the repository root, with recursive submodules initialized:

```sh
git submodule update --init --recursive base upstream/felucca
bash tests/test-fm6-engine.sh
sh build.sh
python3 base/tools/build.py app --host
```

The device package is produced at `app/build/fm1lab.fwsc`. The host build runs the same app code against fm1-base's host platform.

The GitHub Actions workflow **Native FM1 build** uses the pinned fm1-base builder and the matching JieLi toolchain. It runs the FM6 host test before building the device and host targets. The workflow artifact is the device package; the run log reports linker, RAM/pool, XIP-slot and package checks.

## Memory and update safety

fm1-base uses Felucca-BSP's linker, loader, update path and package builder as one consistent system. Its app XIP slot is 581,564 bytes. The old M-VAVE SDK packager's 453,921-byte policy ceiling belongs to a different firmware layout and is not the active build limit. Do not change either packager's constants or repurpose another firmware family's flash map.

The active app storage map is documented in `docs/REBUILD_AUDIT.md`. Preserve Felucca projects, samples, preset banks, autosave, voice bank, global settings and OTA staging. The app's own data objects remain in the explicitly reserved app/user windows.

## Current scope

The active app currently proves the Felucca FM6 audio path, panel input, MIDI note input, basic patch selection/enable and settings storage. The final four-patch HOME/macro/global pages, named Instrument bank, deep editor, two-lane sequencer, FX integration and physical-device validation remain in progress.

The M-VAVE SDK build workflow is retained only as a manual reference; it is not the release device build.
