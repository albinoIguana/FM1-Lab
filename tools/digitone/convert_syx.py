#!/usr/bin/env python3
"""Convert a Digitone sound bank SysEx dump into FM1-Lab four-patch Instruments.

The converter reads the original Digitone sound parameters and writes a portable
FM1-Lab JSON library. It does not redistribute or embed the source SysEx bank.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import re
from pathlib import Path

from inspect_syx import parse_message, split_messages

C_RATIOS = [0.25, 0.50, 0.75, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0,
            8.0, 9.0, 10.0, 11.0, 12.0, 13.0, 14.0, 15.0, 16.0]
A_RATIOS = [0.25, 0.50, 0.75, 1.0, 1.25, 1.50, 1.75, 2.0, 2.25, 2.50,
            2.75, 3.0, 3.25, 3.50, 3.75, 4.0, 4.25, 4.50, 4.75, 5.0,
            5.50, 6.0, 6.50, 7.0, 7.50, 8.0, 8.50, 9.0, 9.50, 10.0,
            11.0, 12.0, 13.0, 14.0, 15.0, 16.0]

# Source Digitone algorithm -> DX7 algorithm and operator-role placement.
# The mappings retain the major carrier/modulator relationships where possible;
# they are deliberate approximations, not one-to-one topology translations.
TOPOLOGIES = {
    0: {"dx7": 0,  "ops": {"C": 1, "A": 2, "B1": 3, "B2": 4}, "second": "B1"},
    1: {"dx7": 1,  "ops": {"C": 1, "A": 2, "B1": 3, "B2": 4}, "second": "B1"},
    2: {"dx7": 2,  "ops": {"C": 1, "A": 2, "B1": 4, "B2": 5}, "second": "B1"},
    3: {"dx7": 3,  "ops": {"C": 1, "A": 2, "B1": 4, "B2": 5}, "second": "B1"},
    4: {"dx7": 11, "ops": {"C": 1, "A": 3, "B1": 4, "B2": 5}, "second": "A"},
    5: {"dx7": 0,  "ops": {"C": 1, "A": 2, "B1": 3, "B2": 4}, "second": "B1"},
    6: {"dx7": 8,  "ops": {"C": 1, "A": 2, "B1": 3, "B2": 5}, "second": "B1"},
    7: {"dx7": 13, "ops": {"C": 1, "A": 2, "B1": 3, "B2": 4}, "second": "B1"},
}


def clamp(value: float, low: int, high: int) -> int:
    return max(low, min(high, int(round(value))))


def decoded_index_for_original_offset(offset: int) -> int:
    """Map a legacy logical SysEx byte offset through the 8-in-7 transport."""
    encoded_offset = offset - 10
    if encoded_offset < 1:
        raise ValueError(f"Parameter offset {offset:#x} precedes packed payload")
    return encoded_offset - (encoded_offset // 8) - 1


def parameter_byte(payload: bytes, relative_offset: int) -> int:
    index = decoded_index_for_original_offset(0x29 + relative_offset)
    if index >= len(payload):
        raise ValueError(f"Parameter offset {relative_offset:#x} is outside payload")
    value = payload[index]
    if value > 127:
        raise ValueError(
            f"Parameter offset {relative_offset:#x} decoded to non-MIDI value {value}"
        )
    return value


def parameter_decimal(payload: bytes, bit: int, flag_offset: int,
                      msb_offset: int, lsb_offset: int,
                      subtract: float = 0.0) -> float:
    flag = parameter_byte(payload, flag_offset)
    msb = parameter_byte(payload, msb_offset)
    lsb = parameter_byte(payload, lsb_offset)
    upper = (flag >> (7 - bit)) & 1
    fraction = int((50.0 / 127.0) * lsb + (50 if upper else 0))
    return round(msb - subtract + fraction / 100.0, 2)


def parameter_lfo(payload: bytes, bit: int, flag_offset: int,
                  msb_offset: int, lsb_offset: int) -> float:
    flag = parameter_byte(payload, flag_offset)
    msb = parameter_byte(payload, msb_offset)
    lsb = parameter_byte(payload, lsb_offset)
    upper = (flag >> (7 - bit)) & 1
    fraction = (100.0 / 127.0) * lsb
    integer = (msb * 2) - (127 if upper else 128)
    return round(integer + fraction / 100.0, 2)


def decode_parameters(payload: bytes) -> dict:
    flag = parameter_byte(payload, 0x29)
    b_msb = parameter_byte(payload, 0x2F)
    b_lsb = parameter_byte(payload, 0x30)
    b_upper = flag & 1
    b_fraction = (int((b_lsb + 127) * (64.0 / 127.0))
                  if b_upper else int((64.0 / 127.0) * b_lsb))
    b_index = b_msb * 128 + b_fraction
    if b_index > 360:
        raise ValueError(f"Digitone B ratio index out of range: {b_index}")

    c_index = parameter_byte(payload, 0x2B)
    a_index = parameter_byte(payload, 0x2D)
    algorithm = parameter_byte(payload, 0x28)
    if algorithm not in TOPOLOGIES or c_index >= len(C_RATIOS) or a_index >= len(A_RATIOS):
        raise ValueError("Unsupported Digitone algorithm or ratio index")

    return {
        "algorithm": algorithm,
        "c_ratio": C_RATIOS[c_index],
        "a_ratio": A_RATIOS[a_index],
        "b1_ratio": C_RATIOS[b_index // 19],
        "b2_ratio": C_RATIOS[b_index % 19],
        "feedback": parameter_byte(payload, 0x36),
        "mix": parameter_byte(payload, 0x38),
        "harmonics": parameter_decimal(payload, 2, 0x31, 0x32, 0x33, 63.0),
        "detune": parameter_decimal(payload, 4, 0x31, 0x34, 0x35),
        "a_attack": parameter_byte(payload, 0x3F),
        "a_decay": parameter_byte(payload, 0x42),
        "a_end": parameter_byte(payload, 0x44),
        "a_level": parameter_byte(payload, 0x46),
        "b_attack": parameter_byte(payload, 0x48),
        "b_decay": parameter_byte(payload, 0x4B),
        "b_end": parameter_byte(payload, 0x4D),
        "b_level": parameter_byte(payload, 0x4F),
        "amp_attack": parameter_byte(payload, 0x8B),
        "amp_decay": parameter_byte(payload, 0x8D),
        "amp_sustain": parameter_byte(payload, 0x8F),
        "amp_release": parameter_decimal(payload, 2, 0x91, 0x92, 0x93),
        "pan": parameter_byte(payload, 0x96),
        "lfo1_destination": parameter_byte(payload, 0x12),
        "lfo1_wave": parameter_byte(payload, 0x16),
        "lfo1_speed": parameter_lfo(payload, 4, 0x01, 0x04, 0x05),
        "lfo1_depth": parameter_lfo(payload, 4, 0x21, 0x24, 0x25),
        "lfo1_fade": parameter_byte(payload, 0x0D),
        "lfo1_mode": parameter_byte(payload, 0x1F),
    }


def dx7_operator_offset(operator: int) -> int:
    # FM1-Lab's expanded voice stores OP6 first and OP1 last.
    return (6 - operator) * 21


def ratio_to_dx7(ratio: float) -> tuple[int, int]:
    # DX7 ratio mode starts at 0.50 for coarse=0; 0.25 is not representable.
    ratio = max(0.50, min(31.99, float(ratio)))
    if ratio < 1.0:
        return 0, clamp((ratio - 0.50) * 200.0, 0, 99)
    coarse = min(31, int(math.floor(ratio)))
    fine = clamp((ratio - coarse) * 100.0, 0, 99)
    return coarse, fine


def dx7_rate(digitone_time: float) -> int:
    # Digitone time grows slower as the value rises; DX7 rate is inverse.
    return clamp(99.0 - float(digitone_time) * 99.0 / 127.0, 0, 99)


def modulation_envelope(attack: int, decay: int, end: int):
    end_level = clamp(end * 99.0 / 127.0, 0, 99)
    return ([dx7_rate(attack), dx7_rate(decay), 0, 0],
            [99, end_level, end_level, end_level])


def amplitude_envelope(attack: int, decay: int, sustain: int, release: float):
    sustain_level = clamp(sustain * 99.0 / 127.0, 0, 99)
    return ([dx7_rate(attack), dx7_rate(decay), 0, dx7_rate(release)],
            [99, sustain_level, sustain_level, 0])


def set_operator(voice: list[int], operator: int, ratio: float, level: float,
                 rates: list[int], levels: list[int], detune: int = 7) -> None:
    at = dx7_operator_offset(operator)
    voice[at:at + 4] = rates
    voice[at + 4:at + 8] = levels
    # Neutral breakpoint, keyboard scaling, curve, rate scaling and sensitivity.
    voice[at + 8:at + 16] = [0] * 8
    voice[at + 16] = clamp(level, 0, 99)
    voice[at + 17] = 0  # ratio mode
    coarse, fine = ratio_to_dx7(ratio)
    voice[at + 18] = coarse
    voice[at + 19] = fine
    voice[at + 20] = clamp(detune, 0, 14)


def convert_voice(payload: bytes, source_name: str):
    p = decode_parameters(payload)
    topology = TOPOLOGIES[p["algorithm"]]
    voice = [0] * 156

    # Digitone MIX crossfades its two carrier outputs. Approximate the balance
    # with a constant-power pair of DX7 carrier levels.
    mix = max(0, min(127, p["mix"]))
    x_level = 99.0 * math.sqrt((127.0 - mix) / 127.0)
    y_level = 99.0 * math.sqrt(mix / 127.0)

    # Digitone HARM is bipolar and affects different operator groups on either
    # side of zero. Approximate this with a restrained ratio shift.
    harmonics = p["harmonics"]
    c_ratio = p["c_ratio"] * (2.0 ** (min(0.0, harmonics) / 104.0))
    a_ratio = p["a_ratio"] * (2.0 ** (max(0.0, harmonics) / 104.0))
    b1_ratio = p["b1_ratio"] * (2.0 ** (max(0.0, harmonics) / 104.0))
    detune = clamp(7.0 + (p["detune"] - 64.0) * 7.0 / 63.0, 0, 14)

    amp_rates, amp_levels = amplitude_envelope(
        p["amp_attack"], p["amp_decay"], p["amp_sustain"], p["amp_release"]
    )
    a_rates, a_levels = modulation_envelope(p["a_attack"], p["a_decay"], p["a_end"])
    b_rates, b_levels = modulation_envelope(p["b_attack"], p["b_decay"], p["b_end"])

    second = topology["second"]
    levels = {
        "C": x_level,
        "A": y_level if second == "A" else p["a_level"] * 99.0 / 127.0,
        "B1": y_level if second == "B1" else p["b_level"] * 99.0 / 127.0,
        "B2": p["b_level"] * 99.0 / 127.0,
    }
    envelopes = {
        "C": (amp_rates, amp_levels),
        "A": (amp_rates, amp_levels) if second == "A" else (a_rates, a_levels),
        "B1": (amp_rates, amp_levels) if second == "B1" else (b_rates, b_levels),
        "B2": (b_rates, b_levels),
    }
    ratios = {"C": c_ratio, "A": a_ratio, "B1": b1_ratio, "B2": p["b2_ratio"]}
    detunes = {"C": 7, "A": detune, "B1": 7, "B2": detune}

    for role, operator in topology["ops"].items():
        rates, levels_eg = envelopes[role]
        set_operator(voice, operator, ratios[role], levels[role], rates, levels_eg, detunes[role])

    # Unused operators are silent but valid, leaving the source's four operators
    # clearly identifiable in the generated voice.
    for operator in range(1, 7):
        if operator not in topology["ops"].values():
            set_operator(voice, operator, 1.0, 0, [0, 0, 0, 0], [0, 0, 0, 0])

    voice[134] = topology["dx7"]
    voice[135] = clamp(p["feedback"] * 7.0 / 120.0, 0, 7)
    voice[136] = 0
    voice[137] = clamp((p["lfo1_speed"] + 128.0) * 99.0 / 255.0, 0, 99)
    voice[138] = clamp(p["lfo1_fade"] * 99.0 / 127.0, 0, 99)
    # Destination/depth encoding is retained in provenance rather than guessed.
    voice[139] = 0
    voice[140] = 0
    voice[141] = 1 if p["lfo1_mode"] else 0
    voice[142] = p["lfo1_wave"] % 6
    voice[143] = 0

    patch_name = re.sub(r"[^A-Z0-9 _-]", " ", source_name.upper()).strip()[:10].ljust(10)
    voice[145:155] = [ord(char) & 0x7F for char in patch_name]
    voice[155] = clamp(p["pan"], 0, 127)

    if len(voice) != 156 or any(value < 0 or value > 127 for value in voice):
        raise ValueError(f"Generated invalid FM1 voice for {source_name}")
    return voice, p


def convert_bank(input_path: Path) -> dict:
    blob = input_path.read_bytes()
    messages = split_messages(blob)
    if not messages:
        raise ValueError("No Digitone SysEx sound messages found")

    sounds = [parse_message(message, index) for index, message in enumerate(messages, 1)]
    if len(sounds) != 124:
        raise ValueError(f"Expected 124 source sounds, found {len(sounds)}")
    if not all(sound["checksum_valid"] and sound["length_valid"] for sound in sounds):
        raise ValueError("Source bank contains a failed checksum or length validation")

    converted = []
    for sound in sounds:
        payload = bytes.fromhex(sound["decoded_payload_hex"])
        voice, params = convert_voice(payload, sound["name"])
        converted.append({
            "sourceIndex": sound["index"],
            "sourceName": sound["name"],
            "sourceTags": sound["tags"],
            "sourceAlgorithm": params["algorithm"],
            "sourceParameters": params,
            "voice": voice,
        })

    instruments = []
    for start in range(0, len(converted), 4):
        group = converted[start:start + 4]
        patches = []
        for sound in group:
            patches.append({
                "name": sound["sourceName"][:10],
                "sourceName": sound["sourceName"],
                "sourceIndex": sound["sourceIndex"],
                "baseData": sound["voice"],
                "macros": {"brightness": 50, "body": 50, "motion": 50, "dirt": 50},
                "selectedOperator": 1,
                "enabled": True,
                "level": 31,
            })
        instrument_number = len(instruments) + 1
        instruments.append({
            "format": "fm1lab-instrument",
            "version": 1,
            "id": f"digitone-{instrument_number:02d}",
            "name": re.sub(r"[^A-Z0-9 _-]", " ", group[0]["sourceName"].upper()).strip()[:16] or f"DN {instrument_number:02d}",
            "patches": patches,
        })

    return {
        "format": "fm1lab-digitone-library",
        "version": 1,
        "source": {
            "fileName": input_path.name,
            "sha256": hashlib.sha256(blob).hexdigest(),
            "device": "Elektron Digitone",
            "soundCount": len(sounds),
            "allChecksumsValid": True,
            "allLengthsValid": True,
            "payloadVersion": 4,
        },
        "conversion": {
            "method": "parameter-aware four-operator to six-operator translation",
            "instrumentCount": len(instruments),
            "patchCount": len(converted),
            "patchesPerInstrument": 4,
            "defaultPatchLevel": 31,
            "algorithmMappings": {str(key): value for key, value in TOPOLOGIES.items()},
            "knownLimitations": [
                "Digitone routing is approximated with DX7 algorithms; no exact graph equivalence is claimed.",
                "Digitone HARM is approximated with bounded operator-ratio shifts.",
                "Only one Digitone LFO can be represented; LFO destination/depth is preserved in metadata, not guessed.",
                "Digitone filter, drive, effects, arpeggiator and controller assignments are not translated.",
                "Ratios below 0.50 are clamped because DX7 ratio mode cannot represent 0.25 exactly.",
            ],
        },
        "instruments": instruments,
        "sourceManifest": [
            {
                "sourceIndex": sound["sourceIndex"],
                "sourceName": sound["sourceName"],
                "sourceTags": sound["sourceTags"],
                "sourceAlgorithm": sound["sourceAlgorithm"],
                "instrumentIndex": (sound["sourceIndex"] - 1) // 4,
                "patchIndex": (sound["sourceIndex"] - 1) % 4,
                "sourceParameters": sound["sourceParameters"],
            }
            for sound in converted
        ],
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path, help="Digitone sound-bank .syx file")
    parser.add_argument("-o", "--output", type=Path, required=True,
                        help="Output FM1-Lab JSON library")
    args = parser.parse_args()
    library = convert_bank(args.input)
    args.output.write_text(json.dumps(library, indent=2, ensure_ascii=True) + "\n",
                           encoding="utf-8")
    print(f"Converted {library['conversion']['patchCount']} sounds into "
          f"{library['conversion']['instrumentCount']} four-patch Instruments: {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
