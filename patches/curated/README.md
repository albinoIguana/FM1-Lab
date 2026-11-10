# Curated FM1 instruments

Each instrument definition is a four-voice set with complementary roles. Link every voice to its stable patch ID in `catalog/patches.csv`. The current draft factory palette is `instrument-bank-v0.3.json`, with its selection rationale in `INSTRUMENT-BLUEPRINT.md`.

The archive is the source of truth; firmware presets are generated outputs. To rebuild the firmware header after changing an approved manifest:

```sh
python3 tools/generate_dx7_library.py
```

Only promote an instrument after all four voices have been auditioned on the FM1-Lab engine, loudness-matched, checked across registers and screened for duplicates. The current v0.3 bank is a listening build: it expands analog-like FM, leads, shimmer leads, pads and motion textures while keeping one FM Rhythm and one Synth Perc instrument. The new source voices still need listening and refinement.

## Mix calibration

Each instrument manifest entry carries a shared `mix_level` for its four layers. The current values are calibrated against the native FM6 renderer at MIDI notes 48, 60 and 72, velocity 127, targeting approximately 6,000,000 Q24 dry peak at maximum MASTER. This keeps the four-layer sum below the output limiter while preserving the relative balance of the four source voices. Re-run `tests/test-fm6-engine.sh` after changing the bank; do not reset every patch to level 31.
