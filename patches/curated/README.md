# Curated FM1 instruments

Each instrument definition is a four-voice set with complementary roles. Link every voice to its stable patch ID in `catalog/patches.csv`. The target 32-instrument palette is in `INSTRUMENT-BLUEPRINT.md`.

Only promote an instrument after all four voices have been auditioned on the FM1-Lab engine, loudness-matched, checked across registers and screened for duplicates. The archive is the source of truth; firmware presets are generated outputs. Keep one FM Rhythm and one Synth Perc instrument, then spend the remaining factory slots on basses, leads, pads, analog-like tones, industrial textures, keys and motion sounds.


The current provisional factory selection is `instrument-bank-v0.2.json` (32 instruments / 128 voices). The compiled device header is generated from that selection. Do not promote new voices to the factory bank solely from their source folder: audition them first.
