# Four-engine patch library and Instrument model

## User-facing contract

Keep the current Instrument screen, four physical slots, slot focus, encoder behavior, mixer controls, and preset browsing interaction. Do not redesign the UI.

One Instrument is exactly four independently selectable engine patches:

1. Slot 1: FM6 / DX7 patch
2. Slot 2: wavetable patch
3. Slot 3: PSG patch
4. Slot 4: subtractive patch

The slot determines the engine. A preset change replaces the selected slot's actual engine patch data and stable patch identity; changing only its displayed name is invalid. The four engine assignments never change when browsing.

## Source library layout

Use parallel engine roots, each with the same sound-family organization and catalog conventions:

```text
patches/library/
  dx7/
    bass/
    leads/
    pads/
    ...
  wavetable/
    bass/
    leads/
    pads/
    ...
  psg/
    bass/
    leads/
    percussion/
    ...
  subtractive/
    bass/
    leads/
    pads/
    ...
```

Each family folder contains a README and machine-readable `patches.json`. Every patch has a stable engine-qualified ID, display name, source/provenance and license metadata, classification/tags, and engine-native parameter payload. Preserve original DX7 assets and provenance when relocating the existing family archive under `dx7/`; do not convert DX7 data into fake non-FM patches.

The three new catalogs must contain real parameterized patches. Empty folders or renamed DX7 voices do not count as completed catalogs. Factory presets should be deliberately small and useful rather than importing hundreds of DX7 voices into every Instrument.

## Runtime data contract

A live Instrument stores four patch records. Each record contains:
- engine-derived slot role (not user-selectable);
- stable engine-local patch ID / catalog index;
- display name;
- engine-native patch parameters;
- shared performance controls (enabled, level, pan, octave/fine tuning, macros).

FM6 retains its expanded 156-byte voice representation. Wavetable, PSG and subtractive records use distinct typed parameter payloads; they must not interpret the legacy DX7 voice bytes as oscillator configuration.

Preset replacement is atomic: validate the catalog entry and engine-specific parameters first, then replace that slot's identity, name and payload together. Preserve slot-owned mix, pan, tuning and macro values when changing the sound, matching current DX7 preset-step behavior.

## DSP contract

Each non-FM engine receives its selected patch parameters at note initialization. The parameters must affect the generated audio, not merely UI metadata. Updating a preset changes the corresponding engine's sound while leaving the other three slots untouched. Existing four-engine voice allocation, 32 total voices, ARP routing, sequencer routing, Maelstrom, and Chorus/Delay/Reverb/Phaser remain intact.

The engine-native parameter model must stay compact and fixed-point/allocation-free in the audio callback. No dynamic allocation or catalog parsing occurs in the audio interrupt.

## Persistence and migration

The current patch-bank record stores packed DX7 VMEM bytes for every slot and cannot represent the three other engines. Introduce a versioned mixed-engine patch-bank format with CRC and explicit engine-native payload encoding. Keep the current format decoder as a one-way migration source:
- slot 1 keeps its existing DX7 patch and identity;
- slots 2–4 receive deterministic, valid engine-native defaults until the user selects a real patch;
- existing names and shared performance controls are retained where valid;
- no legacy DX7 voice payload is treated as a wavetable, PSG or subtractive patch.

Migration must be tested from existing v3 patch banks, including interrupted/invalid records and save/reload round trips. Do not overwrite the old bank until the new record has been written and verified.

## Acceptance tests

- Each of the four slots can select at least two different valid presets from its own catalog.
- For each slot, preset selection changes its audio output measurably and changes its saved identity.
- Changing one slot does not mutate the other three slots.
- Save/reload preserves all four IDs, names, engine parameters and shared performance controls.
- Legacy v3 banks migrate without losing the FM6 patch or shared controls.
- All four engines remain audible together with each of the 16 retained FX enable combinations.
- ARP and Maelstrom remain operational across all four engines.
- UI interaction remains the current four-slot Instrument workflow.
- Native tests, firmware tests, browser emulator, device build and package checks pass before physical audition. No firmware flash before the hardware release gate.
