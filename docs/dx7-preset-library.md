# Curated DX7 preset library

The **archive source of truth** is `patches/`: 896 indexed six-operator voice slots, including 812 named voices and 84 empty source slots. The named archive contains 764 unique voice-data patterns after exact duplicate analysis. Stable IDs, original names (`original_name`), display names, source-bank provenance, licensing and converted 156-byte voice data are retained. Generic names reused by distinct voices receive concise settings-derived display names; byte-identical duplicate copies keep the same display name. See `patches/README.md`, `patches/catalog/patches.csv`, `patches/catalog/empty-slots.csv` and `patches/catalog/duplicates.csv`.

The active FM1 factory candidate is **32 instruments / 128 voices**, generated from `patches/curated/instrument-bank-v0.3.json` into `app/fm1lab_dx7_data.h`. Each instrument contains four voices. All 128 selected voices are unique by exact voice-data comparison, and all IDs resolve to named archive voices.

## Factory palette v0.3

| Slots | Family | Instruments | Voices |
|---|---|---:|---:|
| 01–06 | Bass foundations | 6 | 24 |
| 07 | Choir / vocal | 1 | 4 |
| 08 | FM rhythm | 1 | 4 |
| 09 | Synth percussion | 1 | 4 |
| 10–15 | Analog-like / cleaner techno FM | 6 | 24 |
| 16–23 | Leads, including shimmer and space | 8 | 32 |
| 24–30 | Pads and strings | 7 | 28 |
| 31–32 | Cinematic / glassy motion | 2 | 8 |
| **Total** |  | **32** | **128** |

This is deliberately more melodic and textural than the previous bass/choir/percussion-heavy layout. Percussion is limited to one FM Rhythm and one Synth Perc instrument. The lead section gets eight distinct four-voice sets, while analog-like FM gets six and pads get seven.

## Naming

Home titles are derived from the first patch's original DX7 voice name (the ten-character DX7 name field), rather than the source folder or bank category. Curated instrument labels and all four selected patch names remain in the manifest.

## Source and rights

The 768 additional voices in the archive came from 24 CC0-1.0 banks in the visualizersdotnl DX7 library, covering Analog, MOOG, Lead, Synth Pads and Synth Resonant families. The archive preserves source paths and voice positions.

Modern reference sets are recorded in `patches/catalog/SOURCES.md` and `patches/catalog/sources.csv`. The mene311 FM-1 project provides 26 thematic 32-voice banks and an audio gallery, but its MIT license covers the tooling only; the patch data remains the work of original authors. NatLife's Ambient & Dreams V2: Analog Vibes is a directly FM-1-compatible commercial 32-patch bank with explicit analog emphasis. These are references or external audition candidates, not mirrored into our archive without redistribution rights.

## Curation status

v0.3 is a **draft listening build**, not a claim that all selected voices have been fully auditioned. The 425-entry audition queue is deduplicated by exact voice data and ranked using name hints only. Prioritize the eight lead sets and seven pad sets, then test analog-like and cleaner techno groups. Listen across registers and velocities, loudness-match, check tuning and note-off behavior, and retag by actual sound character. In particular, confirm which voices deliver shimmering leads, warm analog-like movement, clean techno weight and industrial aggression.

## Migration behavior

The previous 32-instrument DX7 factory data is retained in `app/fm1lab_dx7_legacy_data.h` solely for exact-match migration detection. On startup, untouched old DX7 defaults can be replaced by the new factory selection. Renamed or edited user instruments are preserved.
