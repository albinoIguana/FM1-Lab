# Pre-Digitone library backup

This directory snapshots the FM1-Lab library implementation immediately before the Digitone library replacement work.

Included:
- app/app.c: factory Instrument definitions, migration logic, and seeding behavior
- app/fm1lab_instrument.[ch]: Instrument/patch data model and voice conversion
- app/fm1lab_store_library.[ch]: persistent user-library storage format
- firmware/fm1lab_library.c and firmware/fm1lab_bank_metadata.[ch]: native firmware library adapter and metadata format

The original files are copied verbatim as .txt snapshots so a later change to the active implementation does not destroy the prior source.

Important limitation: this is a source-level backup, not a dump of the contents of a physical FM1's flash. The device's user-edited Instruments are stored locally and have not been read from the hardware. The current firmware migration path intentionally preserves named/edited Instruments; it does not silently overwrite them.

The backup was created on main before any Digitone library activation changes. Restore the corresponding files from this directory or revert to the commit immediately preceding this backup series.
