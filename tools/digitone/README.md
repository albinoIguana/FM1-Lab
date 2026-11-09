# Digitone sound import research

FM1-Lab now includes a conservative inspector for original Elektron Digitone sound SysEx messages:

    python3 tools/digitone/inspect_syx.py path/to/digitone-sounds.syx -o digitone-sounds.json

The inspector extracts the sound name and preserves each complete source message plus its parameter region as hex. It intentionally does not claim to convert a Digitone patch into an FM1 patch yet.

## Decoder reference

The parameter research is based on the public [d-huck/libdigitone](https://github.com/d-huck/libdigitone) project (MIT-licensed). That project documents Digitone's sound message layout and parameter byte locations. Its own README describes the decoder as work in progress and says it does not encode Digitone SysEx.

FM1 uses M-VAVE's expanded 156-byte DX7-compatible voice layout and a six-operator engine. Digitone's four-operator topology, algorithm routing, operator envelopes, modulation and performance destinations do not map one-to-one. A conversion must therefore be explicit and testable rather than copying bytes or inventing values.

## Conversion plan

1. Inspect actual .syx sound dumps and validate message boundaries.
2. Decode Digitone parameter values using the documented parameter map.
3. Build a topology-aware mapping from Digitone's four operators to FM1's six operators, preserving carrier/modulator roles where possible.
4. Generate four related FM1 patches per Instrument, with safe output levels and expressive Brightness / Body / Motion / Dirt macros.
5. Render and audition against the original source demos; refine by ear.
6. Keep source names and provenance in a separate manifest.

Do not overwrite factory or user Instruments until the translated bank has passed structural checks and is explicitly reviewed.

## What is needed for rtFM 2

The free rtFM 2 demo or a user-provided full-pack .syx dump is needed to translate those exact sounds. The inspector can process the dump once it is available. Do not redistribute the original pack; the resulting FM1 patches should be independently recreated translations, with source attribution.
