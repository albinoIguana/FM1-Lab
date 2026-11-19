# Dual-core audio scheduling architecture

Status: implementation in progress on `shipwreck-mixed-synthesis`.

## Current stage: measured voice scheduling

The voice renderer records elapsed timer ticks for each active voice, grouped by
fixed engine slot (FM6, wavetable, PSG, subtractive) and by worker core. CPU0
and CPU2 write separate rows; the audio owner reads them only after the worker
has joined. At the end of a callback, per-engine average voice cost and the
per-quantum CPU0 postmix cost are folded into a 3:1 EWMA.

At the start of each 32-frame quantum, voices are stably sorted by the latest
measured engine cost. The CPU0 voice budget is estimated as:

`max(1, (estimated_total_voice_ticks - previous_cpu0_postmix_ticks_per_quantum) / 2)`

The split is then bounded by the existing CPU2 rollback snapshot capacity.
This aims to balance CPU0 voice rendering plus its postmix against CPU2 voice
rendering. It is an adaptive estimate, not a guarantee of equal core utilization.
The first callback uses the legacy engine-cost ratios until measurements exist.

## Measurement rules

- All costs use the same hardware timer and are measured in ticks.
- Per-voice timing covers the full engine render and that voice's patch-mix
  contribution for one 32-frame quantum.
- Postmix is normalized to one quantum before it is compared with per-voice
  costs; comparing a whole callback with one quantum would bias the split.
- CPU2 idle time is not counted as job time. CPU2 is dispatched only when
  there is voice work to render.
- Keep callback wall time, CPU0 active time, CPU2 job time, CPU2 wait time,
  backup/dispatch overhead, and postmix time distinct in diagnostics.

## Next stage: effects partition

The FX path is stateful and currently processes the combined stereo sends after
both voice ranges have completed. It must not be naively run once per core:
that would duplicate effect state and change shared delay/reverb feedback,
chorus/phaser modulation, and stereo-linked dynamics.

The next refactor is to separate:
1. patch-to-stereo dry/send accumulation;
2. stateful send-effect processing with one owner for each effect state;
3. master amp/drive, EQ, stereo-linked compressor, limiter and output.

Only independent blocks may run concurrently. Shared effect state must have a
single writer, and the audio output must preserve the existing order and signal
domain. A CPU2 FX job should overlap useful CPU0 work rather than simply move
the same serial work to the other core and make CPU0 wait.

## Acceptance criteria

- Native FM1 build, engine tests, and browser emulator pass.
- Device measurements show CPU0 voice/postmix, CPU2 voice/job, wait and total
  callback time separately.
- Balanced work does not increase deadline misses or cause dropouts.
- Compare bypass, each FX individually, all FX, and mixed-engine polyphony.
- Preserve the known-good FM6 core, Instrument persistence, and existing sound
  behavior while the scheduling work is introduced.
