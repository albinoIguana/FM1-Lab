# Curated FM1 instruments

Each instrument definition is a four-voice set with complementary roles. Link every voice to its stable patch ID in `catalog/patches.csv`. The current draft factory palette is `instrument-bank-v0.3.json`, with its selection rationale in `INSTRUMENT-BLUEPRINT.md`.

The archive is the source of truth; firmware presets are generated outputs. To rebuild the firmware header after changing an approved manifest:

```sh
python3 tools/generate_dx7_library.py
```

Only promote an instrument after all four voices have been auditioned on the FM1-Lab engine, loudness-matched, checked across registers and screened for duplicates. The current v0.3 bank is a listening build: it expands analog-like FM, leads, shimmer leads, pads and motion textures while keeping one FM Rhythm and one Synth Perc instrument. The new source voices still need listening and refinement.
