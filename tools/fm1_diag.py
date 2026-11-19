#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Record live FM1-Lab audio diagnostics over the FM1's USB-MIDI SysEx link.

Install: python -m pip install mido python-rtmidi
Run from a clone made with submodules:
  python tools/fm1_diag.py --duration 180 --interval 1 --output fm1-session.csv

The firmware must include APP_LOG support. The logger only requests read-only
snapshots; it does not send notes, change settings, or alter presets.
"""
import argparse
import csv
import datetime as dt
import pathlib
import sys
import time

ROOT = pathlib.Path(__file__).resolve().parents[1]
BASE_TOOLS = ROOT / "base" / "tools"
if BASE_TOOLS.is_dir():
    sys.path.insert(0, str(BASE_TOOLS))

try:
    import mido
    import fm1ports
except ImportError as exc:
    raise SystemExit(
        "Missing MIDI dependencies. Run: python -m pip install mido python-rtmidi"
    ) from exc

LOG = [0x7D, 0x4C, 0x47]
END = b"end"


def read_snapshot(inp, out, timeout=1.5):
    # Discard stale replies, then request one atomic-ish firmware snapshot.
    for _ in inp.iter_pending():
        pass
    out.send(mido.Message("sysex", data=LOG))
    deadline = time.monotonic() + timeout
    values = {}
    received = False
    while time.monotonic() < deadline:
        for msg in inp.iter_pending():
            if msg.type != "sysex" or list(msg.data[:3]) != LOG:
                continue
            payload = bytes(msg.data[3:])
            if payload == END:
                return values if received else None
            text = payload.decode("ascii", "replace")
            if "=" in text:
                key, value = text.split("=", 1)
                try:
                    values[key] = int(value)
                    received = True
                except ValueError:
                    pass
        time.sleep(0.002)
    return values if received else None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", help="substring of the FM1 MIDI port name")
    parser.add_argument("--interval", type=float, default=1.0,
                        help="seconds between snapshots (default: 1)")
    parser.add_argument("--duration", type=float, default=0,
                        help="recording length in seconds; 0 runs until Ctrl-C")
    parser.add_argument("--output", default="fm1-diagnostics.csv",
                        help="CSV output path")
    args = parser.parse_args()
    if args.interval < 0.25:
        parser.error("--interval must be at least 0.25 seconds")
    try:
        inp, out, port_name = fm1ports.connect(args.port, names=("FM1-Lab",), mido=mido)
    except LookupError as exc:
        raise SystemExit(f"Could not connect to FM1 over USB-MIDI: {exc}") from exc

    path = pathlib.Path(args.output)
    rows = 0
    print(f"Connected: {port_name}")
    print(f"Recording to: {path.resolve()}")
    print("Start with all FX bypassed, then add load in stages. Ctrl-C stops.")
    try:
        with path.open("w", newline="", encoding="utf-8") as handle:
            writer = None
            start = time.monotonic()
            next_sample = start
            while not args.duration or time.monotonic() - start < args.duration:
                now = time.monotonic()
                if now < next_sample:
                    time.sleep(min(0.05, next_sample - now))
                    continue
                snapshot = read_snapshot(inp, out)
                stamp = dt.datetime.now(dt.timezone.utc).isoformat(timespec="milliseconds")
                if snapshot is None:
                    print(f"{stamp}  no diagnostic reply (is this an APP_LOG firmware build?)")
                else:
                    row = {"host_time_utc": stamp, "port": port_name, **snapshot}
                    if writer is None:
                        fields = list(row.keys())
                        writer = csv.DictWriter(handle, fieldnames=fields)
                        writer.writeheader()
                    writer.writerow(row)
                    handle.flush()
                    rows += 1
                    render = snapshot.get("render_us", 0)
                    budget = snapshot.get("render_budget_us", 0)
                    load = (100.0 * render / budget) if budget else 0.0
                    print(
                        f"{stamp}  render {render}/{budget} us ({load:.1f}%)"
                        f"  CPU0 {snapshot.get('cpu0_busy_us', 0)} us"
                        f"  CPU2 {snapshot.get('cpu2_job_sum_us', 0)} us"
                        f"  voices {snapshot.get('voice_peak', 0)}"
                        f"  xruns {snapshot.get('xruns', 0)}"
                    )
                next_sample += args.interval
                if next_sample < time.monotonic() - args.interval:
                    next_sample = time.monotonic() + args.interval
    except KeyboardInterrupt:
        print("\nStopped.")
    finally:
        inp.close()
        out.close()
    print(f"Saved {rows} diagnostic snapshots to {path.resolve()}")


if __name__ == "__main__":
    main()
