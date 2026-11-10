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

The archive now contains **896 individual six-operator voices** indexed in `catalog/patches.csv`: the original 128 plus 768 additional voices from 24 CC0 DX7 banks. Imported families include analog-emulation (384), leads (96), pads/strings (160), and resonant/motion (128), alongside the original bass, choir and percussion sets. The original 128 still skew heavily toward bass, choir and percussion; the new material directly addresses those gaps.

All imported voices retain original names, source bank, source voice index, CC0 provenance and converted 156-byte voice data. The new imports are **not yet auditioned**; their folders are provisional source-family groupings, not final sound classifications. They must be listened to, deduplicated and retagged before promotion into four-patch FM1 instruments.

See `catalog/sources.csv` for external candidate collections and redistribution status, and `catalog/next-imports.csv` for the first import batch (now ingested).
