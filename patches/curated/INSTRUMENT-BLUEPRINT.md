# Factory instrument curation: v0.3

The active 32-instrument / 128-voice factory candidate is defined in `instrument-bank-v0.3.json`. It uses stable IDs from the sound-family archive and is generated into `app/fm1lab_dx7_data.h`.

## Palette balance

| Slots | Count | Direction |
|---|---:|---|
| 01–06 | 6 | Bass foundations, clean/punchy/funk/moving |
| 07 | 1 | Choir / vocal layer |
| 08–09 | 2 | One FM Rhythm and one Synth Perc instrument |
| 10–15 | 6 | Moog / Oberheim / Prophet / analog-like and cleaner techno FM |
| 16–23 | 8 | Classic 80s, fat, Prophet, digital, modern, new-school, shimmer and space leads |
| 24–30 | 7 | 80s, analog, chorus, cloud, dark, ice and evolving pads |
| 31–32 | 2 | Cinematic and glassy motion textures |

This intentionally replaces the previous 8-choir / 8-FM-rhythm / 8-synth-percussion-heavy balance with much more playable melodic and textural variety. Percussion is now two instruments total.

## Why the new groups

- **Analog-like FM:** the archive contains name-hint candidates such as MINI-MOOG, OBERHEIM, PROPHET, ANALOG BRT and ANLOG SQR. These groups provide a useful starting palette for rounded, subtractive-style tones.
- **Leads:** eight four-voice sets create more room for classic 80s, fat, digital, modern and shimmering lead directions.
- **Pads:** seven sets cover warm, analog-like, chorus, cloud, dark, icy and evolving textures.
- **Motion:** two groups emphasize glassy and cinematic evolving voices.

## Review status

This is a **draft listening build**, not a claim that every group has been fully auditioned. The imported CC0 voices have valid 156-byte records and original source names, but individual sound character still needs listening on the FM1-Lab engine. The first test should focus on the eight lead instruments and seven pad instruments, then check analog-like and cleaner techno groups for loudness, tuning, note-off and duplicate feel.

Empty source slots are excluded from the selection. The complete inventory and candidate audition queue remain in `catalog/patches.csv` and `catalog/audition-queue.csv`.
