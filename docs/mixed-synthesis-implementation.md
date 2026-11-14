# Mixed synthesis implementation plan

Status: implementation branch, design checkpoint. This document does not claim that any engine has been integrated or benchmarked on FM1 hardware.

## Target architecture

An Instrument remains exactly four sound slots. The two sequencer instrument banks remain independent and retain their current 0..3 / 4..7 internal patch contexts. The candidate mixed Instrument is:

1. One four-operator FM engine.
2. One wavetable engine.
3. One subtractive engine.
4. One plucked-string engine.

This is an alternative to four four-operator FM slots, not an additional fifth slot. Engine selection must not change Instrument library indices, preset selection, sequencer lane mapping, or existing save/load migration semantics.

## DSP integration boundary

Keep note allocation, note source tokens, note-off routing, pitch bend, per-slot level/pan, shared sends, master effects, limiter, and callback diagnostics in the existing FM6 wrapper. Add a tagged engine state to each voice and dispatch note init, note-off, render, and voice-done operations by engine type.

The renderer must continue to pack active voices dynamically before CPU0/CPU2 splitting. Engine state must be self-contained or use explicitly partitioned scratch so CPU2 backup/rollback remains correct. Do not add locks, allocation, floating point, or unbounded work to the audio callback.

The current FM6 core remains available for the one FM slot until a measured four-operator core is selected. Muting two operators in a six-operator render is not an acceptable CPU optimization if the core still computes all six operators.

## Candidate engines

### Wavetable
- Fixed-point phase accumulator.
- Shared read-only single-cycle tables; no per-voice table copies.
- Linear interpolation as the baseline; compare nearest-neighbour only if aliasing is acceptable.
- Per-voice envelope and pitch modulation.
- No per-voice effects.

### Subtractive
- Two band-limited or appropriately limited oscillators, mix, and one low-cost resonant-capable filter.
- Fixed-point filter state.
- Keep oscillator and filter oversampling off in the first implementation.
- Per-voice envelope; shared LFO/modulation inputs where practical.

### Plucked string
- Karplus-Strong-style delay loop with damping and fractional tuning only if the measured cost permits.
- Explicitly budget delay memory per active voice before choosing maximum delay length.
- Support bass fundamentals; do not silently cap the delay so high that low notes become unusable.
- No heap allocation and no buffer clearing proportional to maximum delay on every render sample.

## Performance accounting

The existing callback diagnostics are the acceptance source on hardware. At 256 frames / 44.1 kHz:
- Full callback budget: approximately 5,805 microseconds.
- Voice-shedding threshold: approximately 4,934 microseconds.
- DMA guard: approximately 5,079 microseconds.

Compare identical workloads and collect repeated rows for 1, 2, 4, 8, 16, and 32 active voices where the patch configuration allows it. Record renderus, cpu0voice, cpu2jobsum, cpu2waitsum, voicepeak, renderlate, shed count, and memory/map size. Host timing is useful for correctness and relative algorithm profiling only; it is not a substitute for FM1 hardware timing.

The earlier normalized cost figures (wavetable 0.15–0.30, subtractive 0.35–0.65, pluck 0.25–0.55 relative to one 4-op FM voice) are hypotheses for prototype prioritization, not measured results and must not be used as CPU budgets.

## Required gates

1. Baseline current firmware and record device diagnostics.
2. Add standalone native tests for each engine: pitch, envelope, note-off, silence, clipping bounds, and deterministic output.
3. Integrate engine state and dispatch without changing instrument storage or selection.
4. Add regression tests for all four slots, both sequencer banks, repeated-note source tokens, note stealing, pitch bend, and CPU2 split/fallback.
5. Run existing native/regression tests and the JieLi target build.
6. Measure worst-case callback time, shedding, late callbacks, and RAM on physical FM1 hardware with all shared FX enabled.
7. Only then choose the final engine mix and commit any persistent engine-type/preset schema migration.

## Non-goals for the first implementation

- No change to the HOME/SEL instrument-selection interaction.
- No change to the 32-entry Instrument library or its numbering.
- No changes to legacy factory preset migration.
- No new per-engine FX chains; all four slots feed the existing shared instrument FX/master chain.
- No claim of hardware performance based on host benchmarks or CI build success.


## Prototype checkpoint — 2026-11-14

The four-slot Option B now has standalone fixed-point prototypes for all four engine roles:

1. **Four-operator FM:** four phase accumulators, four operator sine evaluations per sample, four simple routing topologies, and fixed feedback.
2. **Wavetable:** 64-sample sine table, Q32 phase accumulator, linear interpolation.
3. **PSG:** pulse oscillator plus deterministic 15-bit LFSR noise.
4. **Subtractive:** two slightly detuned saw oscillators into a one-pole fixed-point low-pass.

The three lightweight candidate engines share the API in `app/dsp/fm1lab_synth_engines.[ch]`; the FM proof-of-path is in `app/dsp/fm1lab_fm4.[ch]`. Each uses a compact attack/sustain/release envelope. The FM4 voice state is 44 bytes; each lightweight voice state is 36 bytes. At eight voices per Instrument slot, the four-slot Option B requires 352 bytes for the FM4 pool plus 864 bytes for the three lightweight pools: **1,216 bytes of voice state total**, excluding the small read-only lookup tables. There is no per-voice table copy, heap allocation, floating-point DSP, or per-voice effect state.

The standalone native tests are included in `tests/test-fm6-engine.sh`. They check silence before note-on, deterministic output for identical voices, distinct outputs/topologies, bounded samples, octave phase-increment behavior, note-off/release completion, invalid engine rejection, and voice-state sizes.

A reproducible host microbenchmark is available as `tests/bench-mixed-synth.sh`. On the current x86_64 KVM development host (AMD EPYC 9V74, GCC 14.2, `-O2`), eight voices per engine rendered in 256-frame blocks for 12,000 blocks per trial, with five trials per process. Across three process launches, observed median ranges were:

| Prototype | Observed median range |
|---|---:|
| Four-op FM prototype | 13.60–14.34 ns / voice-sample |
| Wavetable | 3.83–3.88 ns / voice-sample |
| PSG | 4.24–4.47 ns / voice-sample |
| Subtractive | 3.71–3.94 ns / voice-sample |

These are host measurements only. They show the simple FM4 proof-of-path at roughly 3.5× the per-voice sample cost of these minimal lightweight prototypes on this host, but they do not compare against the existing six-operator FM6 core and must not be converted into FM1 callback budgets. The FM1's CPU0/CPU2 callback diagnostics remain the deciding measurement.

Prototype limitations are deliberate and remain open:
- The FM4 module is a cost/topology proof, not a production DX7-compatible engine: it has four simple routing topologies, fixed operator ratios/feedback, one shared envelope, and a sine-only operator waveform. It does not replace the current FM6 core.
- The wavetable is a sine-only proof of the table/interpolation path, not a multi-table instrument.
- The subtractive saws are not band-limited yet; aliasing and a musical filter control model must be addressed before integration.
- PSG pulse/noise balance is fixed in this first pass.
- The current envelopes are intentionally minimal, not the final FM1 envelope model.
- None of these modules is connected to the live FM6 allocator or device build. Engine-state dispatch, CPU0/CPU2 backup semantics, and a genuine four-op/FM6 performance comparison remain future work.
- Plucked string is deferred until its per-voice delay memory and target cost are justified.

No engine choice, Instrument schema change, preset migration, UI change, or hardware-performance claim is made by this checkpoint.
