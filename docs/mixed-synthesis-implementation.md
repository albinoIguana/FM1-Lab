# Mixed synthesis implementation plan

Status: implementation branch. Fixed-slot mixed-engine dispatch is integrated into the native FM6 voice allocator on this branch; device build, CPU2 regression, and physical FM1 qualification remain required. No hardware performance claim is made.

## Target architecture

An Instrument remains exactly four sound slots. The two sequencer instrument banks remain independent and retain their current 0..3 / 4..7 internal patch contexts. The candidate mixed Instrument is:

1. **Patch 1 — existing six-operator FM6.** Keep the current production FM6 core and its established voice behavior.
2. **Patch 2 — fixed-point wavetable.**
3. **Patch 3 — PSG pulse + noise.**
4. **Patch 4 — two-oscillator subtractive.**

This is an alternative to four four-operator FM slots, not an additional fifth slot. Engine selection must not change Instrument library indices, preset selection, sequencer lane mapping, or existing save/load migration semantics.

## DSP integration boundary

Keep note allocation, note source tokens, note-off routing, pitch bend, per-slot level/pan, shared sends, master effects, limiter, and callback diagnostics in the existing FM6 wrapper. The engine is a fixed property of the physical patch slot: patch-context indices 0 and 4 use FM6; 1 and 5 use wavetable; 2 and 6 use PSG; 3 and 7 use subtractive. Dispatch by `patch_context % 4`, not by a user-selectable engine field or a per-voice engine tag. Each slot owns a homogeneous voice pool and its engine-specific state.

The renderer must continue to pack active voices dynamically before CPU0/CPU2 splitting. Engine state must be self-contained or use explicitly partitioned scratch so CPU2 backup/rollback remains correct. Do not add locks, allocation, floating point, or unbounded work to the audio callback.

The existing six-operator FM6 core remains the permanent Patch 1 engine for this implementation. The four-operator prototype is not in the active instrument architecture; it remains an isolated performance experiment only. Revisit four-operator FM only if physical-device measurements show a compelling need.

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

### PSG
- Fixed-point pulse oscillator plus a deterministic LFSR noise source.
- Keep duty/noise controls independent when the engine is expanded beyond the first prototype.
- Bound oscillator and noise state per voice; no per-voice sample buffers.

### Deferred: plucked string
- Revisit a Karplus–Strong delay loop only if its per-voice delay memory and target callback cost can be justified.
- Preserve usable bass fundamentals; do not silently cap the delay so high that low notes become unusable.
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

1. **FM6:** existing six-operator production engine; no FM4 substitution.
2. **Wavetable:** 64-sample sine table, Q32 phase accumulator, linear interpolation.
3. **PSG:** pulse oscillator plus deterministic 15-bit LFSR noise.
4. **Subtractive:** two slightly detuned saw oscillators into a one-pole fixed-point low-pass.

The three non-FM engines share the API in `app/dsp/fm1lab_synth_engines.[ch]`. The separate four-operator FM proof-of-path in `app/dsp/fm1lab_fm4.[ch]` is retained only for research and is not used by any of the four instrument slots. Each uses a compact attack/sustain/release envelope. The FM4 experimental voice state is 44 bytes; each lightweight voice state is 36 bytes. The active design instead has one existing FM6 pool and three lightweight pools; its final memory cost must be calculated from the actual `fm6_note_t` and integrated layout, not from the FM4 prototype estimate. There is no per-voice table copy, heap allocation, floating-point DSP, or per-voice effect state.

The standalone native tests are included in `tests/test-fm6-engine.sh`. They check silence before note-on, deterministic output for identical voices, distinct outputs/topologies, bounded samples, octave phase-increment behavior, note-off/release completion, invalid engine rejection, and voice-state sizes.

A reproducible host microbenchmark is available as `tests/bench-mixed-synth.sh`. On the current x86_64 KVM development host (AMD EPYC 9V74, GCC 14.2, `-O2`), eight voices per engine rendered in 256-frame blocks for 12,000 blocks per trial, with five trials per process. Across three process launches, observed median ranges were:

| Prototype | Observed median range |
|---|---:|
| Four-op FM prototype | 13.60–14.34 ns / voice-sample |
| Wavetable | 3.83–3.88 ns / voice-sample |
| PSG | 4.24–4.47 ns / voice-sample |
| Subtractive | 3.71–3.94 ns / voice-sample |

These are host measurements only. They show the simple FM4 proof-of-path at roughly 3.5× the per-voice sample cost of these minimal lightweight prototypes on this host. They do not compare against the existing six-operator FM6 core and are not evidence for the active architecture's CPU cost. The FM1's CPU0/CPU2 callback diagnostics remain the deciding measurement.

The earlier four-FM4 versus FM4-plus-three-lightweight arithmetic is superseded by the fixed FM6 + wavetable + PSG + subtractive architecture and must not be used to estimate its performance.

Prototype limitations are deliberate and remain open:
- The FM4 module is a cost/topology proof, not a production DX7-compatible engine: it has four simple routing topologies, fixed operator ratios/feedback, one shared envelope, and a sine-only operator waveform. It does not replace the current FM6 core.
- The wavetable is a sine-only proof of the table/interpolation path, not a multi-table instrument.
- The subtractive saws are not band-limited yet; aliasing and a musical filter control model must be addressed before integration.
- PSG pulse/noise balance is fixed in this first pass.
- The current envelopes are intentionally minimal, not the final FM1 envelope model.
- The three lightweight modules are now connected to the live FM6 allocator. Fixed-slot dispatch, FM6-state preservation, and CPU0/CPU2 backup semantics are implemented in the wrapper. Device build, static memory/stack audit, target callback measurements, and physical FM1 qualification remain required.
- Plucked string is deferred until its per-voice delay memory and target cost are justified.

No engine choice, Instrument schema change, preset migration, UI change, or hardware-performance claim is made by this checkpoint.


## Fixed-slot architecture decision — 2026-11-14

The instrument engine map is fixed and identical for both sequencer banks:

| Physical slot | Engine | Internal patch contexts |
|---|---|---|
| Patch 1 | Existing six-operator FM6 | 0, 4 |
| Patch 2 | Fixed-point wavetable | 1, 5 |
| Patch 3 | PSG pulse + noise | 2, 6 |
| Patch 4 | Two-oscillator subtractive | 3, 7 |

The physical slot determines the engine. There is no engine selector, per-preset engine-type field, per-voice engine tag, or five-engine mode. Instrument changes replace engine parameters only; they do not change slot identity. The four-operator FM module remains a non-production experiment.

Integration is deliberately staged. First, the three lightweight engines gain phase-continuous pitch updates so pitch bend and tuning can be dispatched without phase resets. The live allocator integration must then preserve FM6's existing note state, output scaling, voice-stealing/source-token behavior, render quantum, and CPU2 copy/fallback semantics. No persistent format, instrument selection, UI, or factory migration changes are part of this stage.


## Live allocator integration checkpoint — 2026-11-14

The fixed map is now active in the FM6 wrapper. Each voice pool is homogeneous by physical patch slot; the wrapper dispatches note-on, note-off, pitch updates, render, and completion by the fixed patch-context map. Patch 1 (contexts 0 and 4) continues through the existing FM6 note state and renderer. The other three slots use their own fixed-point synth voice state.

The voice state is a copy-safe union of the existing 244-byte FM6 note and the lightweight voice state, plus a separate DC-blocker accumulator for lightweight engines. The CPU2 backup/rollback copies the complete voice object, so engine state and DC history roll back together. The active-voice list remains dynamically packed before CPU0/CPU2 splitting. The lightweight render path converts Q15 samples into the existing FM6 Q24 patch-mix domain and retains the shared patch gain, fade, pan/send, FX, and master path.

The wavetable/PSG/subtractive engines accept Q4-semitone pitch updates; pitch bend, per-voice tuning, patch transpose, octave, and fine cents update phase increments without resetting oscillator phases. No persistent engine selector or preset-format field was added. The native FM6 test harness exercises the mixed slots through the existing all-patch and CPU2 split/fallback paths; passing CI is still required before treating this integration as validated.
