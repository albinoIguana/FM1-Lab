# Curated DX7 preset library

The **archive source of truth** is `patches/`: 896 individual six-operator voices organized by sound family, with stable IDs, original names, source-bank provenance, licensing and converted 156-byte voice data. See `patches/README.md` and `patches/catalog/patches.csv`.

The active FM1 factory selection is **32 Instruments / 128 voices**, generated from `patches/curated/instrument-bank-v0.2.json` and compiled into `app/fm1lab_dx7_data.h`. Each Instrument contains four voices.

## Current factory balance

| Slots | Family | Instruments | Voices |
|---|---|---:|---:|
| 01–08 | Bass / sub | 8 | 32 |
| 09–16 | Choir / vocal layers | 8 | 32 |
| 17 | FM rhythm | 1 | 4 |
| 18 | Synth percussion | 1 | 4 |
| 19–22 | Analog-like FM | 4 | 16 |
| 23–26 | Leads | 4 | 16 |
| 27–30 | Pads / strings | 4 | 16 |
| 31–32 | Resonant / motion | 2 | 8 |
| **Total** |  | **32** | **128** |

This keeps the eight bass and eight choral sets, cuts the redundant percussion factory slots to one FM Rhythm and one Synth Perc instrument, and reallocates the remaining fourteen instruments to analog-like voices, leads, pads and motion-oriented sounds.

## Naming

Home titles are derived from the first patch's original DX7 voice name (ten-character DX7 name field), rather than the source folder or bank category. The curated label remains in the selection manifest as an editorial fallback.

## Sources and licensing

The initial 128 voices came from selected banks in [visualizersdotnl/Yamaha-DX7-patch-library](https://github.com/visualizersdotnl/Yamaha-DX7-patch-library), which declares CC0-1.0. The 768 added voices were imported from 24 additional 32-voice banks in the same CC0 collection: Analog, MOOG, Lead, Synth Pads and Synth Resonant categories.

Other promising banks are documented in `patches/catalog/SOURCES.md`. Some modern collections are non-redistributable or commercial, so the repository records their metadata and import status without mirroring their patch data.

## Curation status

The archive and factory selection are structurally valid, but the new selections are **provisional and awaiting listening tests**. The source folders are only initial family hints. Before calling the bank fully curated, audition voices across registers and velocities, check loudness/headroom, compare near-duplicates, and retag by actual sound character. In particular, confirm which candidates truly deliver shimmering leads, warm analog-like movement, clean techno weight, and industrial aggression.

## Migration behavior

The previous 32-instrument DX7 factory data is retained in `app/fm1lab_dx7_legacy_data.h` solely for exact-match migration detection. On startup, untouched old DX7 defaults can be replaced by the new 32-instrument selection. Renamed or edited user instruments are preserved.
