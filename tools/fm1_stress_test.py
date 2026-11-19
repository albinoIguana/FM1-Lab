#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Automated two-engine FM1 hardware stress test over USB-MIDI.

Temporarily overrides Maelstrom amount in RAM for OFF/ON A/B testing, then
restores the saved amount. No preset or flash writes. Configure exactly two
ACTIVE patch slots and bypass the other FX before running.
"""
import argparse
import csv
import datetime as dt
import pathlib
import statistics
import sys
import time

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from fm1_diag import read_snapshot  # noqa: E402 (also adds base/tools)

try:
    import mido
    import fm1ports
except ImportError as exc:
    raise SystemExit(
        "Missing MIDI dependencies. Run: python -m pip install mido python-rtmidi"
    ) from exc

CHANNEL = 0
BASE_NOTE = 48
COUNTERS = (
    "deadline_misses", "xruns", "cpu2_fallbacks", "shed_total",
    "cpu2_bad_jobs", "cpu2_gave_up",
)


def send_on(out, notes, velocity=100):
    for note in notes:
        out.send(mido.Message("note_on", channel=CHANNEL, note=note,
                              velocity=velocity))


def send_off(out, notes):
    for note in notes:
        out.send(mido.Message("note_off", channel=CHANNEL, note=note,
                              velocity=0))


def all_off(out):
    out.send(mido.Message("control_change", channel=CHANNEL,
                          control=123, value=0))


def stamp():
    return dt.datetime.now(dt.timezone.utc).isoformat(timespec="milliseconds")


class Run:
    def __init__(self, inp, out, port, output):
        self.inp, self.out, self.port = inp, out, port
        self.path = pathlib.Path(output)
        self.rows = []
        self.missing_snapshots = 0
        self.voice_underfills = []

    def sample(self, phase, requested, timeout=0.35):
        snap = read_snapshot(self.inp, self.out, timeout=timeout)
        if snap is None:
            self.missing_snapshots += 1
            print(f"{stamp()} {phase:14s} NO_DIAGNOSTIC_REPLY")
            return None
        row = {"host_time_utc": stamp(), "phase": phase,
               "requested_notes": requested, "port": self.port, **snap}
        self.rows.append(row)
        render = snap.get("render_us", 0)
        budget = snap.get("render_budget_us", 0)
        load = 100.0 * render / budget if budget else 0.0
        print(
            f"{row['host_time_utc']} {phase:14s} "
            f"render={render}/{budget}us ({load:.1f}%) "
            f"voices={snap.get('voice_peak', 0)} "
            f"late={snap.get('deadline_misses', 0)} "
            f"xruns={snap.get('xruns', 0)} "
            f"fallback={snap.get('cpu2_fallbacks', 0)} "
            f"shed={snap.get('shed_total', 0)}"
        )
        return snap

    def wait_and_sample(self, phase, requested, seconds, interval=0.5):
        end = time.monotonic() + seconds
        next_sample = time.monotonic()
        while time.monotonic() < end:
            now = time.monotonic()
            if now >= next_sample:
                snap = self.sample(phase, requested)
                if snap and requested:
                    expected = requested * 2
                    observed = snap.get("voice_peak", 0)
                    if observed < expected:
                        self.voice_underfills.append(
                            (phase, expected, observed, snap.get("device_ms", -1))
                        )
                next_sample += interval
            else:
                time.sleep(min(0.01, next_sample - now))

    def set_maelstrom(self, enabled, amount=60, timeout=4.0):
        self.out.send(mido.Message("sysex", data=[0x7D, ord("F"), ord("M"), ord("1"), ord("T"), 1 if enabled else 0, amount if enabled else 0]))
        target = amount if enabled else 0
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            snap = read_snapshot(self.inp, self.out, timeout=0.5)
            if snap is not None and int(snap.get("chaos_amount", -1)) == target:
                print("Maelstrom {} verified: amount={}".format("ON" if enabled else "OFF", target))
                self.rows.append({"host_time_utc": stamp(), "phase": "maelstrom-state", "requested_notes": 0, "port": self.port, **snap})
                return
        raise RuntimeError("Maelstrom state did not verify: expected amount={}".format(target))

    def restore_maelstrom(self, original, timeout=4.0):
        self.out.send(mido.Message("sysex", data=[0x7D, ord("F"), ord("M"), ord("1"), ord("T"), 2, 0]))
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            snap = read_snapshot(self.inp, self.out, timeout=0.5)
            if snap is not None and int(snap.get("chaos_amount", -1)) == original:
                print("Maelstrom restored: amount={}".format(original))
                self.rows.append({"host_time_utc": stamp(), "phase": "maelstrom-restore", "requested_notes": 0, "port": self.port, **snap})
                return True
        print("WARNING: restore not confirmed; firmware auto-restores after five minutes.")
        return False

    def held_phase(self, count, seconds, label=""):
        phase = f"{label}hold-{count}"
        notes = list(range(BASE_NOTE, BASE_NOTE + count))
        all_off(self.out)
        send_on(self.out, notes)
        print(f"\n[{phase}] {count} simultaneous notes for {seconds:.1f}s")
        try:
            self.wait_and_sample(phase, count, seconds)
        finally:
            send_off(self.out, notes)
            all_off(self.out)
            time.sleep(0.12)

    def chord_churn(self, seconds, label=""):
        phase = f"{label}chord-churn"
        print(f"\n[{phase}] rotating four-note chords for {seconds:.1f}s")
        active = []
        end = time.monotonic() + seconds
        next_chord = time.monotonic()
        next_sample = next_chord
        root = BASE_NOTE
        while time.monotonic() < end:
            now = time.monotonic()
            if now >= next_chord:
                send_off(self.out, active)
                active = [root, root + 4, root + 7, root + 12]
                send_on(self.out, active)
                root = BASE_NOTE + ((root - BASE_NOTE + 1) % 24)
                next_chord = now + 0.14
            if now >= next_sample:
                self.sample(phase, 4)
                next_sample = now + 0.5
            time.sleep(0.005)
        send_off(self.out, active)
        all_off(self.out)

    def write_csv(self):
        if not self.rows:
            raise RuntimeError("No APP_LOG diagnostic snapshots received.")
        fields = list(dict.fromkeys(k for row in self.rows for k in row))
        with self.path.open("w", newline="", encoding="utf-8") as handle:
            writer = csv.DictWriter(handle, fieldnames=fields)
            writer.writeheader()
            writer.writerows(self.rows)

    def report(self):
        first, last = self.rows[0], self.rows[-1]
        deltas = {
            key: (int(last.get(key, 0)) - int(first.get(key, 0))) & 0xffffffff
            for key in COUNTERS
        }
        render_pct = [
            100.0 * row.get("render_us", 0) / row.get("render_budget_us", 1)
            for row in self.rows if row.get("render_budget_us", 0)
        ]
        mode_loads = {}
        for mode in ("maelstrom-off/", "maelstrom-on/"):
            values = [100.0 * row.get("render_us", 0) / row.get("render_budget_us", 1)
                      for row in self.rows if str(row.get("phase", "")).startswith(mode)
                      and row.get("render_budget_us", 0)]
            mode_loads[mode] = values
        max_voice = max((row.get("voice_peak", 0) for row in self.rows), default=0)
        print("\n========== FM1 STRESS REPORT ==========")
        print(f"CSV: {self.path.resolve()}")
        print(f"Snapshots: {len(self.rows)} | missing replies: {self.missing_snapshots}")
        print(f"Peak callback budget use: {max(render_pct, default=0):.1f}%")
        if render_pct:
            print(f"Median callback budget use: {statistics.median(render_pct):.1f}%")
        for mode, values in mode_loads.items():
            if values:
                print("{}: median={:.1f}% peak={:.1f}%".format(mode[:-1], statistics.median(values), max(values)))
        off_values, on_values = mode_loads["maelstrom-off/"], mode_loads["maelstrom-on/"]
        if off_values and on_values and statistics.median(off_values):
            delta = 100.0 * (statistics.median(on_values) / statistics.median(off_values) - 1.0)
            print("Maelstrom median render-load change: {:+.1f}% (ON vs OFF)".format(delta))
        print(f"Peak simultaneous voices reported: {max_voice}")
        print("Counter deltas: " + ", ".join(f"{k}={v}" for k, v in deltas.items()))
        if self.voice_underfills:
            print(f"Voice underfill samples: {len(self.voice_underfills)}")
            for phase, expected, observed, device_ms in self.voice_underfills[:8]:
                print(f"  {phase}: expected >= {expected}, observed {observed}, device_ms={device_ms}")
        failures = [f"{k} increased by {v}" for k, v in deltas.items() if v]
        if render_pct and max(render_pct) > 85.0:
            failures.append("render callback exceeded 85% of its audio deadline budget")
        if self.missing_snapshots:
            failures.append(f"{self.missing_snapshots} diagnostic snapshots missing")
        if self.voice_underfills:
            failures.append(f"{len(self.voice_underfills)} samples below expected two-engine voice count")
        if failures:
            print("RESULT: FAIL")
            for failure in failures:
                print(f"  - {failure}")
        else:
            print("RESULT: PASS — no scheduling counters or voice-underfill faults observed.")
        print("NOTE: APP_LOG cannot detect every analog/sample-level click or clipping event.")
        return 1 if failures else 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", help="substring of the FM1 USB-MIDI port name")
    parser.add_argument("--output", default="fm1-two-engine-stress.csv")
    parser.add_argument("--phase-seconds", type=float, default=5.0,
                        help="seconds per sustained-note phase (default: 5)")
    parser.add_argument("--churn-seconds", type=float, default=10.0,
                        help="seconds of rapid chord turnover (default: 10)")
    args = parser.parse_args()
    if args.phase_seconds < 2 or args.churn_seconds < 2:
        parser.error("phase durations must be at least 2 seconds")

    try:
        inp, out, port_name = fm1ports.connect(
            args.port, names=("FM1-Lab",), mido=mido)
    except LookupError as exc:
        raise SystemExit(f"Could not connect to FM1 over USB-MIDI: {exc}") from exc

    run = Run(inp, out, port_name, args.output)
    result = 1
    try:
        print(f"Connected: {port_name}")
        baseline = run.sample("preflight", 0)
        if baseline is None:
            raise RuntimeError("No APP_LOG response. Use an APP_LOG-enabled firmware build.")
        active = baseline.get("patch_active_mask")
        fx = baseline.get("fx_enabled_mask")
        if active is None or fx is None:
            raise RuntimeError("Firmware lacks patch_active_mask/fx_enabled_mask telemetry. Flash the diagnostic build.")
        if int(active).bit_count() != 2:
            raise RuntimeError(
                f"Preflight rejected: patch_active_mask=0x{int(active):x}; "
                "exactly two patch slots must be ACTIVE."
            )
        if int(fx) != 0:
            raise RuntimeError(
                f"Preflight rejected: fx_enabled_mask=0x{int(fx):x}; "
                "bypass every FX before this zero-FX test."
            )
        original_chaos = int(baseline.get("chaos_amount", -1))
        if original_chaos < 0 or original_chaos > 100:
            raise RuntimeError("Firmware lacks valid chaos_amount telemetry; flash the updated diagnostic build.")
        print(f"Preflight OK: active patch mask=0x{int(active):x}; other FX mask=0x{int(fx):x}; saved Maelstrom amount={original_chaos}")
        print("Automated A/B: two-engine polyphony and chord churn with Maelstrom OFF, then ON.")
        on_amount = max(original_chaos, 60)
        try:
            for enabled, label in ((False, "maelstrom-off/"), (True, "maelstrom-on/")):
                run.set_maelstrom(enabled, on_amount)
                run.wait_and_sample(label + "idle", 0, 2.0)
                for count in (1, 2, 4, 8, 12):
                    run.held_phase(count, args.phase_seconds, label)
                run.chord_churn(args.churn_seconds, label)
                all_off(out)
                run.wait_and_sample(label + "release", 0, 2.0)
        finally:
            all_off(out)
            run.restore_maelstrom(original_chaos)
        run.write_csv()
        result = run.report()
    except KeyboardInterrupt:
        print("\nInterrupted; sending All Notes Off.")
        all_off(out)
        if run.rows:
            run.write_csv()
            result = run.report()
    except Exception as exc:
        all_off(out)
        print(f"\nABORTED: {exc}", file=sys.stderr)
        if run.rows:
            run.write_csv()
            run.report()
        result = 2
    finally:
        all_off(out)
        inp.close()
        out.close()
    return result


if __name__ == "__main__":
    raise SystemExit(main())
