#!/usr/bin/env python3
"""Inspect original Elektron Digitone sound SysEx dumps.

This is a conservative extraction tool, not a sound converter. It preserves the
Digitone payload verbatim so a later, tested translator can map it to FM1's
156-byte six-operator voice format without silently guessing at parameters.

Usage:
  python3 tools/digitone/inspect_syx.py input.syx [-o patches.json]
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path

HEADER = bytes((0xF0, 0x00, 0x20, 0x3C, 0x0D, 0x00, 0x53, 0x01, 0x01))
MESSAGE_SIZE = 0x152
NAME_START, NAME_END = 0x18, 0x29
DATA_START, DATA_END = 0x29, 0x14E
CHECK_START = 0x14F


def printable_name(raw: bytes) -> str:
    # Digitone names occupy a fixed-width region; trim padding, retain safe ASCII.
    decoded = raw.decode("ascii", errors="replace")
    return "".join(ch if 32 <= ord(ch) <= 126 else " " for ch in decoded).strip()


def extract_messages(blob: bytes) -> list[bytes]:
    messages: list[bytes] = []
    cursor = 0
    while True:
        start = blob.find(HEADER, cursor)
        if start < 0:
            break
        end = start + MESSAGE_SIZE
        if end > len(blob):
            raise ValueError(f"Truncated Digitone sound message at byte {start}")
        message = blob[start:end]
        if message[CHECK_START:CHECK_START + 2] != bytes((0x02, 0x48)):
            raise ValueError(
                f"Unexpected Digitone sound marker at byte {start}: "
                f"{message[CHECK_START:CHECK_START + 2].hex()}"
            )
        if message[-1] != 0xF7:
            raise ValueError(f"Missing SysEx terminator at byte {end - 1}")
        messages.append(message)
        cursor = end
    return messages


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path, help="Digitone .syx file")
    parser.add_argument("-o", "--output", type=Path, help="JSON output (default: stdout)")
    args = parser.parse_args()

    blob = args.input.read_bytes()
    messages = extract_messages(blob)
    if not messages:
        raise SystemExit("No Digitone sound messages found. Is this a sound dump (not a project/pattern dump)?")

    sounds = []
    for index, message in enumerate(messages, 1):
        sounds.append({
            "index": index,
            "name": printable_name(message[NAME_START:NAME_END]),
            "device": "Elektron Digitone",
            "format": "digitone-sysex-sound",
            "message_length": len(message),
            "header_hex": message[:len(HEADER)].hex(),
            "name_region_hex": message[NAME_START:NAME_END].hex(),
            "parameter_region_hex": message[DATA_START:DATA_END].hex(),
            "parameter_region_length": DATA_END - DATA_START,
            "trailer_hex": message[0x14E:].hex(),
            "source_message_hex": message.hex(),
        })

    result = {
        "format": "fm1lab-digitone-source-dump",
        "version": 1,
        "source_file": args.input.name,
        "sound_count": len(sounds),
        "sounds": sounds,
        "notice": (
            "Source data is preserved verbatim. No Digitone-to-FM1 parameter "
            "conversion has been applied."
        ),
    }
    rendered = json.dumps(result, indent=2, ensure_ascii=True) + "\n"
    if args.output:
        args.output.write_text(rendered, encoding="utf-8")
        print(f"Extracted {len(sounds)} Digitone sounds to {args.output}")
    else:
        print(rendered, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
