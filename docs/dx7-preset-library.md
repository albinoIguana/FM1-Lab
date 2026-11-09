# Curated DX7 preset library

The active factory library contains **32 Instruments / 128 six-operator patches**. Each Instrument is four consecutive DX7 voices, converted from standard 32-voice DX7 SysEx VMEM banks into the FM1-Lab 156-byte voice format.

## Sources and license

All source files below come from [visualizersdotnl/Yamaha-DX7-patch-library](https://github.com/visualizersdotnl/Yamaha-DX7-patch-library), whose repository declares **CC0-1.0**. The source archive describes itself as a preservation collection of freely shared DX7 patch banks. The converted data is retained in `app/fm1lab_dx7_data.h`.

- Bass: `!Instruments/Plucked/Bass/BASS_05.SYX` (voices 6, 7, 14), `BASS_07.SYX` (voice 27), and `BASS_09.SYX` (voices 1–16 and 18–29).
- Choir: `!Instruments/Voice/Choir/CHOIR01.SYX` (all 32 voices).
- FM rhythm: `Yamaha-DX7-Drum-Sounds-Coffeeshopped/DX7-Cfshpd-Drum-Voices.syx` (all 32 voices).
- Synth percussion: `!Instruments/Percussion/Synth Drums/SYNDR01.SYX` (all 32 voices).

## Instrument map

| Slots | Collection | Purpose |
|---|---|---|
| 01–08 | FM BASS | Includes a dedicated sub-focused Instrument in slot 01, plus synth, punchy, moving and plucked basses |
| 09–16 | CHORAL LAYERS | Choir, chorus and sustained vocal textures for layering |
| 17–24 | FM RHYTHM | Kicks, snares, hats, toms and tuned/percussive FM hits |
| 25–32 | SYNTH PERC | Electronic drums, Simmons-style toms, claps and synthetic percussion |

Slot 01 begins with **JH.SUSBS 1**, **JH.SUSBS 2**, **LOW -SPACE**, and **SO*LOW! #1**. These are source patch names, not newly synthesized replacements.

## Patch descriptions

The short descriptions shown on the Home cards are FM1-Lab's own role hints inferred from the patch names and source bank category. They are not claims that the original archive supplied detailed author notes. Original ten-character DX7 voice names are preserved in the patch data.

## Migration behavior

At startup, the library seeder replaces untouched factory placeholders and exact previous Digitone/factory seed Instruments with this DX7 collection. It does not overwrite renamed or edited user Instruments. The previous Digitone source data remains in the repository as a migration reference; it is no longer part of the active preset browser.

PRESET now always cycles the 32 four-patch Instruments, including when a patch is focused. This removes the old Home-screen behavior where PRESET switched to a separate 16-entry M-VAVE recipe list and made the main library difficult to discover.
