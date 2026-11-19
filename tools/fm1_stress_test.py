#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Run a repeatable four-slot FM1 mixed-engine MIDI stress test.

Install: python -m pip install mido python-rtmidi
Run:     python tools/fm1_stress_test.py --port FM1 --output fm1-stress.csv

The test sends ordinary MIDI notes to the currently loaded Instrument. It does
not edit or save presets. Each note is routed by firmware to all four physical
patch slots (FM6, wavetable, PSG, subtractive); patch_enabled_mask must report
all four enabled for the run to qualify as a four-engine test.
"""
import argparse
import csv
import datetime as dt
import pathlib
import sys
import time

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from fm1_diag import read_snapshot, LOG, END  # noqa: E402

try:
    import mido
    import fm1ports
except ImportError as exc:
    raise SystemExit(
        "Missing MIDI dependencies. Run: python -m pip install mido python-rtmidi"
    ) from exc

PHASES = (("idle", 0, 3.0), ("1-voice", 1, 5.0),
          ("4-voice", 4, 5.0), ("8-voice", 8, 5.0),
          ("12-voice", 12, 5.0))
CHANNEL = 0
BASE_NOTE = 48


def send_notes(out, count, velocity=100):
    notes = list(range(BASE_NOTE, BASE_NOTE + count))
    for note in notes:
        out.send(mido.Message("note_on", channel=CHANNEL, note=note,
                              velocity=velocity))
    return notes


def release_notes(out, notes):
    for note in notes:
        out.send(mido.Message("note_off", channel=CHANNEL, note=note,
                              velocity=0))
    out.send(mido.Message("control_change", channel=CHANNEL,
                          control=123, value=0))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", help="substring of the FM1 MIDI port name")
    parser.add_argument("--output", default="fm1-stress.csv",
                        help="CSV diagnostics output")
    parser.add_argument("--voices", type=int, default=12,
                        choices=(1, 4, 8, 12),
                        help="highest simultaneous-note phase (default: 12)")
    parser.add_argument("--phase-seconds", type=float, default=5.0,
                        help="duration of each voiced phase (default: 5)")
    args = parser.parse_args()
    if args.phase_seconds < 2.0:
        parser.error("--phase-seconds must be at least 2 seconds")

    phases = [("idle", 0, 3.0)] + [
        (f"{n}-voice", n, args.phase_seconds)
        for n in (1, 4, 8, 12) if n <= args.voices
    ]
    try:
        inp, out, port_name = fm1ports.connect(
            args.port, names=("FM1-Lab",), mido=mido)
    except LookupError as exc:
        raise SystemExit(f"Could not connect to FM1 over USB-MIDI: {exc}") from exc

    path = pathlib.Path(args.output)
    rows, samples, notes = [], 0, []
    print(f"Connected: {port_name}")
    print("Running baseline, then 1/4/8/12-note mixed-engine phases.")
    print("No preset writes. MIDI All Notes Off is sent on exit.")
    try:
        for phase, count, seconds in phases:
            release_notes(out, notes)
            notes = send_notes(out, count) if count else []
            print(f"\n[{phase}] {seconds:.1f}s")
            deadline = time.monotonic() + seconds
            next_sample = time.monotonic()
            while time.monotonic() < deadline:
                now = time.monotonic()
                if now < next_sample:
                    time.sleep(min(0.02, next_sample - now))
                    continue
                snapshot = read_snapshot(inp, out, timeout=0.35)
                stamp = dt.datetime.now(dt.timezone.utc).isoformat(
                    timespec="milliseconds")
                if snapshot is None:
                    print(f"{stamp}  no APP_LOG reply")
                else:
                    row = {"host_time_utc": stamp, "phase": phase,
                           "requested_voices": count, "port": port_name,
                           **snapshot}
                    rows.append(row)
                    samples += 1
                    enabled = snapshot.get("patch_enabled_mask", -1)
                    render = snapshot.get("render_us", 0)
                    budget = snapshot.get("render_budget_us", 0)
                    pct = 100.0 * render / budget if budget else 0.0
                    print(
                        f"{stamp} {phase:8s} render={render}/{budget}us "
                        f"({pct:.1f}%) voice_peak={snapshot.get('voice_peak', 0)} "
                        f"enabled=0x{enabled:x} "
                        f"late={snapshot.get('deadline_misses', 0)} "
                        f"xruns={snapshot.get('xruns', 0)} "
                        f"cpu2_fallbacks={snapshot.get('cpu2_fallbacks', 0)}"
                    )
                next_sample += 0.5
                if next_sample < time.monotonic() - 0.5:
                    next_sample = time.monotonic() + 0.5
            release_notes(out, notes)
            notes = []
            time.sleep(0.15)
    except KeyboardInterrupt:
        print("\nInterrupted; releasing notes.")
    finally:
        try:
            release_notes(out, notes)
        finally:
            inp.close()
            out.close()

    if not rows:
        raise SystemExit("FAIL: no diagnostic snapshots received; CSV not written")
    fields = list(dict.fromkeys(k for row in rows for k in row))
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)

    enabled_masks = {r.get("patch_enabled_mask", -1) for r in rows}
    max_voice = max(r.get("voice_peak", 0) for r in rows)
    late = max(r.get("deadline_misses", 0) for r in rows)
    xruns = max(r.get("xruns", 0) for r in rows)
    fallbacks = max(r.get("cpu2_fallbacks", 0) for r in rows)
    max_load = max(
        (100.0 * r.get("render_us", 0) / r.get("render_budget_us", 1))
        for r in rows if r.get("render_budget_us", 0)
    )
    print(f"\nSaved {samples} snapshots: {path.resolve()}")
    print(f"Peak render/budget: {max_load:.1f}% | peak voices: {max_voice}")
    print(f"Deadline-miss counter: {late} | XRUN counter: {xruns} | "
          f"CPU2 fallback counter: {fallbacks}")
    if enabled_masks != {15}:
        print("INCONCLUSIVE: all four patch slots were not confirmed enabled "
              f"(observed masks: {sorted(enabled_masks)}).")
    elif late or xruns or fallbacks:
        print("FAIL: audio scheduling/CPU2 fault counters increased during test.")
    else:
        print("PASS: all four slots enabled; no deadline misses, XRUNs, or "
              "CPU2 fallbacks observed.")
    print("Note: telemetry detects scheduling faults, not audible clipping "
          "directly; audio capture is needed to prove sample-level artifacts.")


if __name__ == "__main__":
    main()
