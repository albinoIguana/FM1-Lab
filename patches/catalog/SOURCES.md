# Source and license registry

This registry distinguishes source discovery from permission to redistribute files in this repository.

| Source / collection | Contents and value | License / redistribution | Action |
|---|---|---|---|
| visualizersdotnl/Yamaha-DX7-patch-library | Existing archive source; broad community bank preservation | Repository declares CC0-1.0 | Existing 128 voices indexed; mine additional category-specific banks |
| The Introvert — Deep DX | AI-generated banks: 11 algorithm-focused banks, 3 mixed banks, 3 piano banks, 2 pad banks; modern pad and algorithm studies | May use in music and derivatives, but may not redistribute as a patch/sample library | External candidate; audition locally, do not mirror |
| Solyaris Music — 393 DX7II patches | 15 bundles including analog-style synths, strings, bass, brass, percussion, ambient pads and more | License/redistribution terms not clearly stated on source page | High-priority candidate; obtain permission or user-supplied copy before committing |
| SoundEngine — Flywheel: DX7 | 64 voices specifically marketed as analog emulations plus FM edge; Prophet/Oberheim-style references | Paid commercial bank; redistribution not permitted absent explicit license | Strong analog-like candidate; do not mirror |
| Nick Klimenko — FM Depths | 32 ambient/analog-like tones: silky strings, evolving drones, smooth pads, progressive basses, solo leads, sequences | Commercial soundset; redistribution not permitted absent explicit license | Strong analog-like candidate; do not mirror |

| mene311 — FM-1 banks | 26 themed banks × 32 voices (832 total), with a playable audio gallery; synthwave, acid/techno, leads, ambient, industrial, sci-fi, electronic bass and more | MIT applies to generator/tooling only; patch data remains the work of original authors | Reference-only; do not mirror patch data without separate rights clearance |

| Phi Mu Labs — Origin | 128 original DX7 SysEx patches, programmed in Dexed; broad FM palette | Paid/fair-price product; redistribution not permitted absent explicit license | Candidate for purchase/audition; do not mirror |
| Yamaha Black Boxes / factory ROM banks | Factory DX7 and DX7II banks, including synth, complex and effects categories | Source page provides downloads; individual rights status needs confirmation | Reference only pending rights review |
| Bobby Blues / patches.fm | Very large searchable historical DX7 archives | Rights vary or are not consistently stated | Discover individual banks; do not bulk-import without provenance review |

## Research links

- https://github.com/visualizersdotnl/Yamaha-DX7-patch-library
- https://theintrovert.net/dx7-sound-banks/
- https://solyaris.altervista.org/dx7.html
- https://soundengine.com/product/flywheel-dx7/
- https://lfo.store/shop/software-synths/arturia-dx7-v/fm-depths-soundset-32-presets/
- https://blacksidedsun.gumroad.com/l/pmlo
- https://yamahablackboxes.com/collection/yamaha-dx7-synthesizer/patches/
- https://www.bobbysblues.com/
- https://patches.fm/

## Import gate

Before adding a source bank:
1. Confirm standard DX7 six-operator voice format and compatibility.
2. Record author, bank title, original URL, download date, checksum, voice count and license.
3. Preserve original bank bytes unchanged under `sources/<source-id>/original/`.
4. Extract individual voices into the sound-family folders, retaining original slot and voice name.
5. Deduplicate by normalized voice bytes, then audition. Do not delete near-duplicates solely by byte hash; edited variants may be musically distinct.
6. If redistribution is unclear or prohibited, store only the source record and local import instructions, not the patch data.


## Modern FM-1-specific reference

`mene311/fm1-banks` contains 26 thematic 32-voice DX7 banks with a useful audio gallery and modern directions such as synthwave pads, acid/techno, leads, ambient and industrial. Its MIT license covers its tools, not the patch data; the author states that the patches remain the original authors' work. It is registered as reference-only in `sources.csv` and must not be copied into our archive without rights clearance. See `sources/FM1-BANKS-REFERENCE.md`.
