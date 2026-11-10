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

The initial 128 voices are indexed in `catalog/patches.csv`. They are the existing CC0-1.0 archive selection, not 128 newly auditioned or diverse sounds. Their source banks currently skew heavily toward bass, choir and percussion. This is why the next curation pass must bring in more leads, pads, analog-like tones, keys, motion and textures.

See `catalog/sources.csv` for external candidate collections and redistribution status.
