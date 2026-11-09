# Digitone sound import research

FM1-Lab includes an inspector for original Elektron Digitone sound SysEx dumps:

    python3 tools/digitone/inspect_syx.py path/to/digitone-sounds.syx -o digitone-sounds.json

The inspector:
- Splits concatenated Digitone sound messages.
- Validates the Elektron header, checksum, and length fields.
- Decodes the Digitone 8-in-7 packed payload.
- Extracts sound names and tag labels.
- Preserves decoded synthesis bytes and original messages as hex.

It deliberately keeps the inspection and conversion stages separate. The converter uses the decoded parameter map below and writes an FM1-Lab library JSON file:

    python3 tools/digitone/convert_syx.py path/to/digitone-sounds.syx -o digitone-fm1lab.json

For the supplied URRRS x Digitone bank, this produces 31 four-patch Instruments containing all 124 sounds. The JSON contains 156-byte FM1 voices, neutral macro defaults, conservative patch levels, and a source manifest with original names and decoded parameters. The source .syx itself is not copied into the output or repository.

## Verified source bank

A user-provided copy of the public URRRS x Digitone bank was inspected locally:
- 124 sound messages.
- 361 bytes per message.
- 302 decoded payload bytes per sound.
- All 124 checksums and length fields valid.
- Payload magic `BE EF BA CE`, payload version 4.

The source file itself is not included in this repository. The inventory and converter tooling should not redistribute the original sound bank.

## Decoder references

- [d-huck/libdigitone](https://github.com/d-huck/libdigitone) (MIT) documents Digitone sound parameter offsets and tag metadata. Its README describes the parser as work in progress; it predates the 8-in-7 correction documented below.
- [NoiseAndMatter/DNX SysEx format notes](https://github.com/NoiseAndMatter/DNX/blob/main/docs/sysex-format.md) documents the Elektron container, checksum, length, and MSB-first 8-in-7 codec. Its notes distinguish Digitone sound payload versions and sizes.
- [Elektron Digitone User Manual](https://www.elektron.se/wp-content/uploads/2024/09/Digitone_User_Manual_ENG_OS1.41_231108.pdf), Appendix A, documents the four-operator engine, eight algorithms, C/A/B1/B2 operator groups, two operator envelopes, ratios, harmonics, detune, feedback, and X/Y carrier mix.

## Conversion model

The converter reads the Digitone's original algorithm, C/A/B1/B2 ratios, B ratio pair, feedback, MIX, HARM, DTUN, A/B modulation envelopes, amp envelope, pan, and selected LFO 1 controls. The decoded parameter offsets follow the documented reverse-engineering work in [libdigitone](https://github.com/d-huck/libdigitone), with the 8-in-7 transport unpacked before addressing the logical sound fields.

The mapping is intentionally conservative:
- C and the algorithm's second carrier are mapped to the two DX7 carrier outputs where possible.
- A, B1, and B2 retain their source ratios and modulation-envelope shapes.
- Digitone MIX becomes a constant-power balance between the two FM1 carrier levels.
- Feedback, detune, pan, and one LFO's speed/wave/fade are translated into corresponding FM1 fields.
- Every converted patch starts at level 31 with neutral Brightness / Body / Motion / Dirt macros.

The converter records the source parameters and the exact mapping choice in the JSON manifest so the translation can be audited and improved. It does not embed the source SysEx bytes.

## Conversion considerations

FM1 uses M-VAVE's expanded 156-byte DX7-compatible voice layout and a six-operator engine. Digitone uses four operators grouped as C, A, B1, and B2, with two macro envelopes and two carrier outputs. The conversion therefore needs an explicit topology mapping; copying bytes or simply assigning ratios is not a faithful translation.

A practical conversion should:
1. Decode the Digitone algorithm and identify its carrier/modulator graph.
2. Map C, A, B1, and B2 ratios, envelopes, modulation levels, harmonics, detune, feedback, and X/Y mix into a suitable six-operator DX7 algorithm.
3. In this first pass, leave unmapped FM1 operators silent rather than inventing extra modulation paths. A later revision can use them for reinforcement or spectral motion after audition.
4. Translate filter/amp/LFO and send parameters only where FM1 has a meaningful equivalent; keep unsupported source behavior documented.
5. Group four related translated voices into each FM1 Instrument, set conservative patch levels, and preserve the source names/provenance in a separate manifest.
6. Validate every generated 156-byte voice and four-patch Instrument record; the converter performs these range and structure checks before writing JSON.

Do not overwrite factory or user Instruments while the conversion is experimental. Keep the imported collection separate until it has been reviewed and tested on the emulator and physical FM1. The generated JSON is a portable library artifact; it is not yet a native flash-bank installer.
