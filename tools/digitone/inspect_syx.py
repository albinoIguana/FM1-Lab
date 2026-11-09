#!/usr/bin/env python3
"""Inspect Elektron Digitone sound SysEx dumps, including 8-in-7 decoding.

Usage:
  python3 tools/digitone/inspect_syx.py input.syx [-o patches.json]

The output preserves the decoded payload and original message bytes. It does not
convert Digitone synthesis parameters into FM1/DX7 voices.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

HEADER = bytes((0xF0, 0x00, 0x20, 0x3C, 0x0D, 0x00, 0x53, 0x01, 0x01))
SYSEX_START, SYSEX_END = 0xF0, 0xF7
PAYLOAD_START = 0x0A
TRAILER_LENGTH = 5
PAYLOAD_MAGIC = bytes.fromhex("beefbace")

TAGS = (
    "KICK", "SNAR", "DEEP", "BRAS", "STRI", "PERC", "HHAT", "CYMB",
    "EVOL", "EXPR", "BASS", "LEAD", "PAD", "TXTR", "CRD", "SFX",
    "ARP", "METL", "ACOU", "ATMO", "NOIS", "GLCH", "HARD", "SOFT",
    "DARK", "BRGT", "UNTG", "EPIC", "FAIL", "LOOP", "MINE", "FAV",
)
TAG_LOOK = {
    0: {"1": [23], "2": [31], "3": [23, 31]},
    1: {"4": [7], "8": [15], "c": [7, 15]},
    2: {"1": [28], "2": [29], "3": [28, 29], "4": [30], "5": [30, 28], "6": [29, 30], "7": [28, 29, 30]},
    3: {"1": [24], "2": [25], "3": [24, 25], "4": [26], "5": [24, 26], "6": [25, 26], "7": [24, 25, 26], "8": [27], "9": [24, 27], "a": [25, 27], "b": [24, 25, 27], "c": [26, 27], "d": [24, 25, 27], "e": [24, 26, 27], "f": [24, 25, 26, 27]},
    4: {"1": [20], "2": [21], "3": [20, 21], "4": [22], "5": [20, 22], "6": [21, 22], "7": [20, 21, 22]},
    5: {"1": [16], "2": [17], "3": [16, 17], "4": [18], "5": [16, 18], "6": [17, 18], "7": [16, 17, 18], "8": [19], "9": [16, 19], "a": [17, 19], "b": [16, 17, 19], "c": [18, 19], "d": [16, 17, 19], "e": [16, 18, 19], "f": [16, 17, 18, 19]},
    6: {"1": [12], "2": [13], "3": [12, 13], "4": [14], "5": [12, 14], "6": [13, 14], "7": [12, 13, 14]},
    7: {"1": [8], "2": [9], "3": [8, 9], "4": [10], "5": [8, 10], "6": [9, 10], "7": [8, 9, 10], "8": [11], "9": [8, 11], "a": [9, 11], "b": [8, 9, 11], "c": [10, 11], "d": [8, 9, 11], "e": [8, 10, 11], "f": [8, 9, 10, 11]},
    8: {"1": [4], "2": [5], "3": [4, 5], "4": [6], "5": [4, 6], "6": [5, 6], "7": [4, 5, 6]},
    9: {"1": [0], "2": [1], "3": [0, 1], "4": [2], "5": [0, 2], "6": [1, 2], "7": [0, 1, 2], "8": [3], "9": [0, 3], "a": [1, 3], "b": [0, 1, 3], "c": [2, 3], "d": [0, 1, 3], "e": [0, 2, 3], "f": [0, 1, 2, 3]},
}


def decode_8in7(encoded: bytes) -> bytes:
    """Unpack Elektron's MSB-first groups of seven bytes into eight wire bytes."""
    if len(encoded) % 8 == 1:
        raise ValueError("Malformed 8-in-7 payload: trailing MSB byte has no data")
    decoded = bytearray()
    for offset in range(0, len(encoded), 8):
        group = encoded[offset:offset + 8]
        msb = group[0]
        for index, value in enumerate(group[1:]):
            decoded.append(value | (((msb >> (6 - index)) & 1) << 7))
    return bytes(decoded)


def read14(data: bytes) -> int:
    return (data[0] << 7) | data[1]


def split_messages(blob: bytes) -> list[bytes]:
    messages: list[bytes] = []
    cursor = 0
    while True:
        start = blob.find(bytes((SYSEX_START,)), cursor)
        if start < 0:
            break
        end = blob.find(bytes((SYSEX_END,)), start + 1)
        if end < 0:
            raise ValueError(f"Unterminated SysEx message at byte {start}")
        messages.append(blob[start:end + 1])
        cursor = end + 1
    return messages


def decode_name(payload: bytes) -> str:
    # Digitone names are 16 bytes; Latin-1 preserves the extended character set.
    return payload[12:28].split(b"\x00", 1)[0].decode("latin-1").strip()


def decode_tags(message: bytes) -> tuple[list[str], str]:
    # The original Digitone tag field is encoded as ten hexadecimal nibbles.
    tag_bytes = message[0x12:0x18]
    tag_code = (tag_bytes[:1] + tag_bytes[2:]).hex()
    found: list[str] = []
    for position, nibble in enumerate(tag_code):
        for tag_index in TAG_LOOK.get(position, {}).get(nibble, []):
            tag = TAGS[tag_index]
            if tag not in found:
                found.append(tag)
    return found, tag_code


def parse_message(message: bytes, index: int) -> dict:
    if len(message) < PAYLOAD_START + TRAILER_LENGTH:
        raise ValueError(f"Message {index} is too short ({len(message)} bytes)")
    if message[:len(HEADER)] != HEADER:
        raise ValueError(f"Message {index} is not a Digitone sound dump")
    if message[-1] != SYSEX_END:
        raise ValueError(f"Message {index} has no F7 terminator")

    encoded = message[PAYLOAD_START:-TRAILER_LENGTH]
    stored_checksum = read14(message[-5:-3])
    computed_checksum = sum(encoded) & 0x3FFF
    stored_length = read14(message[-3:-1])
    computed_length = (len(message) - 10) & 0x3FFF
    if stored_checksum != computed_checksum:
        raise ValueError(f"Message {index} checksum mismatch: {stored_checksum} != {computed_checksum}")
    if stored_length != computed_length:
        raise ValueError(f"Message {index} length mismatch: {stored_length} != {computed_length}")

    payload = decode_8in7(encoded)
    if not payload.startswith(PAYLOAD_MAGIC):
        raise ValueError(f"Message {index} has unexpected decoded payload magic: {payload[:4].hex()}")
    if len(payload) < 28:
        raise ValueError(f"Message {index} decoded payload is truncated")

    tags, tag_code = decode_tags(message)
    return {
        "index": index,
        "object_number": message[9],
        "name": decode_name(payload),
        "tags": tags,
        "tag_code_hex": tag_code,
        "device": "Elektron Digitone",
        "format": "digitone-sysex-sound",
        "message_length": len(message),
        "payload_version": int.from_bytes(payload[4:8], "big"),
        "decoded_payload_length": len(payload),
        "payload_header_hex": payload[:28].hex(),
        "synthesis_payload_hex": payload[28:].hex(),
        "decoded_payload_hex": payload.hex(),
        "source_message_hex": message.hex(),
        "checksum_valid": True,
        "length_valid": True,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path, help="Digitone .syx file")
    parser.add_argument("-o", "--output", type=Path, help="JSON output (default: stdout)")
    args = parser.parse_args()

    blob = args.input.read_bytes()
    messages = split_messages(blob)
    if not messages:
        raise SystemExit("No SysEx messages found")
    sounds = [parse_message(message, index) for index, message in enumerate(messages, 1)]
    if len(sounds) != len(messages):
        raise SystemExit("Not all messages were parsed as Digitone sound dumps")

    result = {
        "format": "fm1lab-digitone-source-dump",
        "version": 2,
        "source_file": args.input.name,
        "source_sha256": hashlib.sha256(blob).hexdigest(),
        "sound_count": len(sounds),
        "all_checksums_valid": all(sound["checksum_valid"] for sound in sounds),
        "all_lengths_valid": all(sound["length_valid"] for sound in sounds),
        "sounds": sounds,
        "notice": (
            "Digitone 8-in-7 payloads have been decoded and names/tags extracted. "
            "Synthesis bytes are preserved; no Digitone-to-FM1 parameter conversion "
            "has been applied."
        ),
    }
    rendered = json.dumps(result, indent=2, ensure_ascii=True) + "\n"
    if args.output:
        args.output.write_text(rendered, encoding="utf-8")
        print(f"Validated and extracted {len(sounds)} Digitone sounds to {args.output}")
    else:
        print(rendered, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
