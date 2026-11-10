# Factory instrument curation blueprint

This is the target palette for the next factory-library revision. It is a **selection plan**, not an approved instrument bank: each four-voice group must be assembled from stable patch IDs in the library and auditioned on the FM1-Lab engine before it replaces factory presets.

## Proposed 32-instrument palette

| Slots | Instrument direction | Candidate source families | Curation intent |
|---|---|---|---|
| 01–04 | Bass foundations | bass, analog-emulations | Deep sub, rounded mono bass, punchy FM bass, clean techno bass |
| 05–08 | Modern bass / industrial | bass, analog-emulations, sequences-motion | Acid bite, growl, metallic bass, aggressive industrial low end |
| 09–12 | Classic synth leads | leads, analog-emulations | 80s solo lead, bright sync lead, fat Prophet/Moog-like lead, expressive glide-style voice |
| 13–16 | Modern / shimmering leads | leads, pads-strings, sequences-motion | Shimmer lead, glassy high lead, evolving digital lead, bold modern techno lead |
| 17–20 | 80s and analog-style pads | pads-strings, analog-emulations | Warm analog-like pad, wide chorus pad, slow string pad, soft poly pad |
| 21–24 | Modern pads and atmospheres | pads-strings, sequences-motion | Shimmering pad, evolving cloud, cinematic bed, animated motion pad |
| 25–27 | Textures and sequences | sequences-motion, textures-drones-fx | Industrial machine texture, resonant sequence voice, dark cinematic drone |
| 28–29 | Keys and plucks | keys-electric-piano, plucks-mallets, bells-metallic | Distinctive FM keys, glass/metal plucks |
| 30 | Vocal / ensemble | vocal-choir, pads-strings | Choir or formant texture, not another near-duplicate choir bank |
| 31 | FM Rhythm | percussion-fm | One dedicated tuned-FM rhythm instrument |
| 32 | Synth Percussion | percussion-synth | One dedicated electronic percussion instrument |

## Four-patch assembly rules

- Four patch positions should have distinct musical jobs, not four near-identical source-bank variations.
- A proposed group should cover useful registers and have balanced loudness/headroom.
- Include at least one patch with clear transient/attack, one sustained or evolving voice, and one contrasting harmonic character where the concept allows.
- Do not use blank source slots, duplicates, or unreviewed voices as final selections.
- Retain original voice names and patch IDs in the manifest; instrument display names are our own concise editorial names.
- Keep percussion to one FM Rhythm and one Synth Perc instrument. The remaining slots go to melodic and textural FM sounds.

## Current gate

The 896-entry archive is indexed. 84 imported entries are empty source slots and are explicitly marked `empty-source-slot`; 812 entries remain named candidates. The new voices have not yet been auditioned on the FM1-Lab engine. Use `catalog/audition-queue.csv` to start with analog-like, shimmer/pad, lead, aggressive and motion candidates. Do not promote the plan into firmware until the selected voices have passed listening and regression checks.
