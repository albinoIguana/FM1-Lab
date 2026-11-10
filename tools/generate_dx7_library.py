#!/usr/bin/env python3
"""Generate the FM1-Lab DX7 firmware bank from the curated patch archive."""
from __future__ import annotations

import argparse
import json
from pathlib import Path

ALPHABET = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ.-:+=^!/*?&<>()[]{}@%$#"
DESCRIPTIONS = {
    "bass": "FM BASS",
    "vocal-choir": "VOCAL LAYER",
    "percussion-fm": "FM RHYTHM",
    "percussion-synth": "SYNTH PERC",
    "analog-emulations": "ANALOG-LIKE FM",
    "leads": "SYNTH LEAD",
    "pads-strings": "SYNTH PAD",
    "sequences-motion": "MOTION TEXTURE",
}


def z85_encode(data: list[int]) -> str:
    if len(data) % 4:
        raise ValueError("voice data length must be divisible by four")
    encoded = []
    for offset in range(0, len(data), 4):
        value = int.from_bytes(bytes(data[offset:offset + 4]), "big")
        digits = [""] * 5
        for index in range(4, -1, -1):
            value, remainder = divmod(value, 85)
            digits[index] = ALPHABET[remainder]
        if value:
            raise ValueError("Z85 group overflow")
        encoded.append("".join(digits))
    return "".join(encoded)


def c_string(value: str) -> str:
    return '"' + value.replace("\\", "\\\\").replace('"', '\\"') + '"'


def main() -> None:
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser()
    parser.add_argument("--manifest", default="patches/curated/instrument-bank-v0.3.json")
    parser.add_argument("--output", default="app/fm1lab_dx7_data.h")
    args = parser.parse_args()

    manifest = json.loads((root / args.manifest).read_text(encoding="utf-8"))
    library = {}
    for path in (root / "patches/library").rglob("*.json"):
        payload = json.loads(path.read_text(encoding="utf-8"))
        for patch in payload.get("patches", []):
            patch_id = patch["id"]
            if patch_id in library:
                raise ValueError(f"duplicate patch ID: {patch_id}")
            library[patch_id] = patch

    instruments = manifest["instruments"]
    if len(instruments) != 32:
        raise ValueError("firmware bank must contain exactly 32 instruments")
    mix_levels = [item.get("mix_level", 31) for item in instruments]
    if any(not isinstance(level, int) or level < 1 or level > 127 for level in mix_levels):
        raise ValueError("each instrument mix_level must be an integer from 1 to 127")

    voices: list[list[int]] = []
    names: list[str] = []
    descriptions: list[str] = []
    used_ids: set[str] = set()
    for expected_slot, instrument in enumerate(instruments, 1):
        if instrument["slot"] != expected_slot:
            raise ValueError("instrument slots must be ordered 1..32")
        if len(instrument["patch_ids"]) != 4:
            raise ValueError(f"instrument {expected_slot} must have four patches")
        label = instrument["label"]
        if len(label.encode("ascii")) > 23:
            raise ValueError(f"instrument label too long: {label!r}")
        names.append(label)
        description = DESCRIPTIONS.get(instrument["category"], "FM VOICE")
        for patch_id in instrument["patch_ids"]:
            if patch_id in used_ids:
                raise ValueError(f"patch reused in multiple slots: {patch_id}")
            used_ids.add(patch_id)
            patch = library.get(patch_id)
            if not patch:
                raise ValueError(f"patch not found in archive: {patch_id}")
            voice = patch["voice_156"]
            if len(voice) != 156 or any(not isinstance(x, int) or x < 0 or x > 127 for x in voice):
                raise ValueError(f"invalid 156-byte DX7 voice: {patch_id}")
            voices.append(voice)
            descriptions.append(description)

    if len(voices) != 128:
        raise ValueError("firmware bank must contain exactly 128 voices")
    encoded = [z85_encode(voice) for voice in voices]
    if any(len(item) != 195 for item in encoded):
        raise ValueError("unexpected Z85 voice size")

    voice_lines = []
    for item in encoded:
        voice_lines.extend(c_string(item[i:i + 78]) for i in range(0, len(item), 78))
    header = [
        "#ifndef FM1LAB_DX7_DATA_H",
        "#define FM1LAB_DX7_DATA_H",
        "",
        "#define FM1LAB_DX7_INSTRUMENT_COUNT 32u",
        "#define FM1LAB_DX7_VOICE_COUNT 128u",
        "#define FM1LAB_DX7_Z85_BYTES 195u",
        "",
        "/* Generated from patches/curated/instrument-bank-v0.3.json.",
        " * Patch source of truth: patches/library/ by audible sound family.",
        " * 128 voices / 32 four-patch instruments. Draft selections require listening.",
        " */",
        "static const char fm1lab_dx7_voice_z85[] =",
    ]
    header.extend("    " + line for line in voice_lines)
    header[-1] += ";"
    header += [
        "",
        "static const char fm1lab_dx7_instrument_names[FM1LAB_DX7_INSTRUMENT_COUNT][24] = {",
    ]
    header.extend("    " + c_string(name) + "," for name in names)
    header += [
        "};",
        "",
        "static const unsigned char fm1lab_dx7_instrument_level[FM1LAB_DX7_INSTRUMENT_COUNT] = {",
    ]
    header.extend("    " + str(level) + "," for level in mix_levels)
    header += [
        "};",
        "",
        "static const char fm1lab_dx7_patch_descriptions[FM1LAB_DX7_VOICE_COUNT][20] = {",
    ]
    header.extend("    " + c_string(description) + "," for description in descriptions)
    header += ["};", "", "#endif", ""]
    output = root / args.output
    output.write_text("\n".join(header), encoding="utf-8")
    print(f"Wrote {output.relative_to(root)}: {len(voices)} voices / {len(instruments)} instruments")


if __name__ == "__main__":
    main()
