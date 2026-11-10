# FM1-Lab patch archive

This directory is the **source of truth for patch assets and curation metadata**. The device factory library is a generated selection, never the master archive.

## Organization

- `catalog/` — machine-readable patch inventory, source registry and taxonomy.
- `library/` — patch assets organized by audible sound family, not by author or download source.
- `sources/` — provenance, source-bank notes, license terms and original bank references.
- `curated/` — approved four-patch FM1 instrument sets, assembled from the library.

## Rules

1. Every patch gets a stable ID, original voice name, source attribution, license status, and one primary sound-family folder.
2. Keep the original patch name and source-bank provenance. Our category/tag labels are editorial metadata, not claims made by the original author.
3. Keep raw source assets immutable. Any edited/normalized version is a derivative with its own ID and parent reference.
4. Do not commit paid banks, non-redistributable banks, or files with unclear rights. Track those as external candidates until permission or a user-supplied licensed copy is available.
5. A patch is not "curated" until it has been auditioned on the FM1-Lab engine, checked across useful registers, and compared for loudness, playability and redundancy.
6. Four-patch instruments are curated separately from the single-voice archive. The four voices should have complementary musical roles.

## Current inventory

The archive contains **896 indexed voice slots**: 128 original factory candidates plus 768 additional entries from 24 CC0 DX7 banks. Of those, **84 are empty source slots**, explicitly excluded from curation; **812 named voices remain**. New archive families include analog-emulation candidates, leads, pads/strings and resonant/motion, filling out the previous bass/choir/percussion-heavy selection.

Every imported voice retains its original name, source-bank path, voice index, CC0 provenance and converted 156-byte data. The family folders are starting classifications, not a substitute for listening. None of the new voices has yet been auditioned on the FM1-Lab engine. Use `catalog/audition-queue.csv` for a prioritized, name-hint-only listening queue, then record actual sound traits before promoting voices into `curated/`.

A modern external reference is documented in `sources/FM1-BANKS-REFERENCE.md`. It has useful synthwave, acid/techno, ambient, lead and industrial themes, but its bank-data rights remain with the original authors, so it is reference-only. See `catalog/sources.csv` for all source and licensing status.
