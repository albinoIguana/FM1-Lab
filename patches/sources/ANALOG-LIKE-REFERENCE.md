# Analog-like and modern DX7 references

The goal is not to make FM imitate a subtractive synth by name alone. We want the audible traits: rounded fundamentals, restrained upper partials, slow filter-like spectral movement, warm detune/chorus, smooth envelopes, and playable leads/pads that do not default to the classic bright DX bell attack.

## Strong references found

| Collection | What it contributes | Rights / handling |
|---|---|---|
| [FM-1 Banks](https://github.com/mene311/fm1-banks) | 26 FM-1-focused 32-voice banks: synthwave pads/bass, acid/techno, leads, ambient textures, industrial, sci-fi, bells, keys and more | Reference-only. Tooling is MIT; bank voices remain attributed to their original authors. |
| [FM Depths](https://lfo.store/shop/software-synths/arturia-dx7-v/fm-depths-soundset-32-presets/) | 32 custom 6-op-compatible sounds: silky strings, evolving drones, smooth pads, analog-like bass, solo leads, voice-like sounds and sequences | Commercial. Do not copy into our archive without permission. Excellent sound-design target for the analog-like gap. |
| [Flywheel: DX7](https://soundengine.com/product/flywheel-dx7/) | 64 sounds, including Prophet/Oberheim-inspired tones, warm pads, resonant sweeps, basses and leads | Commercial. Do not redistribute. Its patch-name list is useful as a listening/design reference. |
| [NatLife Ambient & Dreams V2 – Analog Vibes](https://natlifesounds.com/product/ambient-dreams-v2-analog-vibrations-for-m-vave-fm-1-fm-synthesizers/) | 32 FM-1-targeted presets, with analog emphasis across bass, leads, pads, plucks and synths | Commercial. Do not copy into our archive. Especially relevant because it is made for the FM-1. |
| [The Introvert – Deep DX](https://theintrovert.net/dx7-sound-banks/) | AI-generated DX7 banks covering algorithm studies, pads, pianos and mixed FM sounds | The author permits use in music but explicitly prohibits redistribution as a patch/sample library. Reference/audition only. |

## What we can safely curate now

The type-organized CC0 archive already contains 384 analog-emulation candidates, including banks labelled Analog and MOOG, plus analog-like candidates scattered among leads and pads. The names are only clues: a patch called MOOG is not automatically a good Minimoog emulation. We still need listening tests for warmth, envelope shape, low-register stability, velocity response and useful polyphony.

The draft factory manifest `curated/instrument-bank-v0.3.json` selects four-voice groups from the CC0 archive and deliberately includes:
- Minimoog-, Oberheim- and Prophet-inspired candidate instruments
- 80s-style and modern leads, including shimmer/space leads
- Classic, analog-like, chorus, cloud, dark, ice and evolving pads
- One FM Rhythm and one Synth Perc instrument

The manifest is marked `draft-un-auditioned`. It is a structured starting palette, not a claim that these 128 voices have already been heard on the FM1.

## Audition criteria before promotion

1. Compare against a reference sound at matched loudness; do not select by patch name alone.
2. Test low, middle and high registers, both single notes and sustained chords.
3. Listen dry first, then with the FM1's own chorus, phaser, delay and reverb. Do not rely on a lush demo reverb to disguise a thin source patch.
4. Reject near-duplicates even if their source names differ.
5. Record actual tags (warm, silky, brass-like, PWM-like, sync-like, glassy, metallic, industrial, evolving, etc.) and an audition status in the archive record.
6. Only then promote a four-patch set into the factory manifest.

**Recommendation:** the most promising analog-like references are FM Depths, Flywheel and NatLife Analog Vibes. We should not import their paid patch data; instead, use their demos as targets and select or program legally redistributable CC0 voices that achieve the same musical roles.
