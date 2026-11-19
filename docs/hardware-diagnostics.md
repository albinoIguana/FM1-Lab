# FM1 hardware diagnostics

The firmware exposes a read-only diagnostic snapshot through fm1-base's existing
USB-MIDI development log protocol. It does not send notes or change presets,
settings, or flash.

## Requirements

- An FM1 running an FM1-Lab build with `APP_LOG` enabled.
- A USB data cable connected directly to a computer.
- Python 3.10 or newer.
- The repository cloned with submodules:
  `git clone --recurse-submodules https://github.com/albinoIguana/FM1-Lab.git`
- MIDI dependencies: `python -m pip install mido python-rtmidi`.

## Record a session

From the repository directory:

```sh
python tools/fm1_diag.py --duration 180 --interval 1 --output fm1-session.csv
```

Omit `--duration` (or set it to 0) to record until Ctrl-C. If more than one
MIDI device is connected, select the FM1 port with `--port "name fragment"`.

The program requests one snapshot per interval over USB-MIDI SysEx and writes
one CSV row per completed snapshot. It prints a compact live summary. It never
opens the FM1 as a disk, sends MIDI notes, or changes device state.

## Captured measurements

The CSV includes callback render time and budget, render peaks and deadline
misses, CPU0 busy/voice/postmix/FX/output time, CPU2 job and wait time (sum and
maximum), CPU2 backup cost, UI/MIDI/master-gain timing, active voice peak, render
quanta, CPU2 dispatch/fallback counts, platform CPU load and clock, DMA xruns,
overload voice shedding, pre-limiter peak, limiter gain reduction, CPU2
health/stack status, the active patch-slot mask, and the enabled-FX mask. Both
host UTC time and the device millisecond uptime are recorded.

The per-callback values are the most recent completed audio callback at the
instant the snapshot is requested. Cumulative counters such as xruns,
deadline misses, and shed count are since boot. The host timestamp is the
time the snapshot was received, not the exact audio callback timestamp.

## Suggested hardware run

Keep the same Instrument and playing pattern for each stage. Record at least
30 seconds per stage, then add the next load:

1. Idle, all FX bypassed.
2. A few held notes, all FX bypassed.
3. Increasing chord/polyphony, all FX bypassed.
4. Both sequencer tracks active.
5. Add chorus, then delay, then reverb, then phaser.
6. Enable Amp/Drive, EQ, and master compressor.
7. Repeat the heaviest musical case and check whether xruns, deadline misses,
   CPU2 fallbacks, or voice shedding increase.

Save a separate CSV per stage if you want clean comparisons. Do not use the
diagnostic logger as a substitute for listening for clicks, dropouts, stuck
notes, or timing errors.


## Automated two-engine stress test

With exactly two HOME slots marked ACTIVE and all four FX bypassed, run:

```sh
python tools/fm1_stress_test.py --output fm1-two-engine-stress.csv
```

The script verifies the active-slot and FX masks before sending any notes. It
then runs idle, 1/2/4/8/12-note sustained phases, rapid four-note chord turnover,
and a release check. It sends All Notes Off on completion or interruption and
does not edit presets/settings. The report fails on new deadline misses, DMA
xruns, CPU2 fallback/bad-job/health faults, overload voice shedding, missing
diagnostic replies, or observed voice counts below the two-engine expectation.

This is a device-side scheduling and voice-path test. APP_LOG cannot detect every
analog clipping event or sample-level discontinuity; if the counters pass while
the output remains audibly broken, record the FM1 analog output through an audio
interface for waveform-level analysis.


### Automated Maelstrom A/B stress test

The updated tools/fm1_stress_test.py automatically runs the two-engine polyphony and four-note chord-churn workload with Maelstrom OFF and ON. It uses a runtime-only APP_SYSEX test command to set chaos_amount, verifies the reported amount through APP_LOG before proceeding, captures the original value, and restores it after the test. The firmware also restores the captured value after five minutes if the host disconnects. This command does not set the settings dirty flag and does not write presets or flash.

The test requires the matching firmware build with APP_SYSEX and chaos_amount telemetry. It still requires a physical FM1 for actual device timing/audio measurements; host tests cannot prove analog output quality.
