# Audio Engine 2.0 — Architecture Contract

Status: **design freeze in progress**. This document defines target interfaces and invariants before further processor implementation. It is intentionally separate from visual/UI direction. No visual redesign is authorized.

## 1. Product invariants

1. Exactly four independently playable patches belong to one Instrument.
2. A patch's level is stable and independent of how many other patches are sounding. Adding or removing another layer must not silently change the gain of an existing layer.
3. Factory/default patch levels are no greater than 25% linear amplitude.
4. Default master gain staging target is **−12 dBFS** for the defined four-patch coherent worst-case reference signal.
5. The final peak limiter is a separate safety processor, with a nominal ceiling near **−1 dBFS**. It is not the mechanism used to establish normal operating gain.
6. No intermediate bus may clip and rely on the final limiter or soft clipper to hide it.
7. All DSP in the audio callback is fixed-memory, allocation-free, bounded, and free of locks, flash access, UI work, logging, or blocking operations.
8. Existing user instruments, patch levels, macros, patterns, and supported V1–V4 settings must remain recoverable through explicit migrations. Safe defaults apply to factory/new state; valid user-authored levels must not be silently overwritten.

## 2. Reference gain model

### Patch level

- Patch level is a normalized **linear-amplitude** control, not a dB or power control.
- Persistent/UI range remains 0–127 for compatibility in the current control surface.
- Runtime conversion is patch_gain = level / 127.
- 0 means exact silence and must contribute no dry signal and no send signal.
- Factory/new Instrument default: 31/127 = 0.2441 (approximately 24.4%), satisfying the 25% maximum.
- Full scale 127 remains available as an intentional user setting.
- Remove active-layer-count normalization from the patch gain calculation. The same patch level must produce the same gain whether one, two, three, or four patches are active.

### Master trim and reference target

- Add a distinct default master trim of **−12.0 dB**, represented internally in the selected fixed-point format (Q15 amplitude multiplier approximately 0.25119, or integer 8231 when rounded).
- The physical MASTER control remains a separate user attenuation/level control; it must not erase or bypass the default safety trim.
- Reference test: four identical, fully correlated, full-scale patch signals at 25% linear amplitude each sum to approximately unity before master trim. The −12 dB master trim then yields approximately −12 dBFS at the output.
- The reference target is a documented gain-staging test condition, not a promise that arbitrary musical material or resonant FX returns will always peak at exactly −12 dBFS.
- Any master compressor makeup gain or user master gain must be accounted for explicitly in the gain budget and metering.

### Final safety stage

- Keep musical compression and peak limiting as separate processors.
- Target a deterministic final ceiling around **−1 dBFS** (approximately 0.89125 linear amplitude).
- Limiter input/output and gain reduction must be observable in diagnostics.
- The limiter must not conceal clipping in patch strips, send buses, returns, or the master summing accumulator.
- The existing soft-clip stage is not considered a substitute for a specified peak limiter.

## 3. Canonical signal topology

    Patch 1 voice pool ─> patch strip 1 ─┬─> dry mixer ──────────────┐
    Patch 2 voice pool ─> patch strip 2 ─┤                          │
    Patch 3 voice pool ─> patch strip 3 ─┤                          ├─> master dynamics
    Patch 4 voice pool ─> patch strip 4 ─┘                          │    compressor
              │                                                     │    peak limiter
              └─> per-patch sends ─> Chorus ─┐                      │    master trim
                                    Phaser ──┤─> stereo FX return ──┘    stereo output
                                    Delay ───┤
                                    Reverb ──┘

Each patch strip owns voice-pool output, stable patch level, pan, Amp/Drive insert and independent bypass, three-band EQ insert and independent bypass, and smoothed parameter transitions.

The four shared FX buses own independent processor state and independent send levels from each patch. The return architecture is stereo-capable even where an individual effect is mono-in/stereo-out. Dry and wet paths remain separately measurable and summable.

## 4. Stereo and routing contract

- Internal patch-strip representation is stereo before the send split, so pan has one canonical location and FX sends can preserve the intended spatial image.
- Each patch has four independent send controls: Chorus, Phaser, Delay, Reverb.
- Each send bus has a documented input format, internal headroom, wet-output format, and return gain.
- FX return summing occurs in a wider accumulator than the final output sample format.
- A bus may apply documented normalization/return trim, but must not silently depend on active patch count.
- Mono effects must document their mono fold-down rule; stereo effects must document their L/R behavior and correlation assumptions.
- Feedback loops are bounded and tested independently from the dry path.

## 5. Parameter and state ownership

Separate state into three lifetimes:

1. **Instrument state** — four patch voices, patch names, patch levels, pan, inserts, sends, and instrument-level musical sound settings. This travels with the Instrument.
2. **Performance state** — sequencer patterns/assignments, arp settings, tempo and performance controls intended to be saved with the performance/session.
3. **Device state** — physical master setting, global MIDI/device preferences, diagnostics configuration, and other hardware-specific settings.

The precise serialization boundary must be reconciled with the existing four-voice Instrument bank metadata before any schema is changed. Do not duplicate authoritative values between the settings record and Instrument bank without a documented ownership rule.

Persistence requirements:
- Validate a complete candidate before applying any of it.
- Keep CRC protection and explicit schema versions.
- Preserve V1–V4 imports; add a new version only after state ownership and field layout are frozen.
- Migrations preserve valid user-authored values. Only newly created factory/default instruments receive the safe 31/127 starting level.
- Transport runtime, voice state, delay buffers, filter histories, and limiter detector histories are not serialized unless a future requirement explicitly says otherwise.
- Invalid or truncated records must leave active state unchanged.

## 6. Smoothing and real-time behavior

- Continuous gain, pan, send, wet, filter, drive, EQ, compressor, and master parameters must use a documented smoothing strategy.
- Use sample-accurate or bounded block-ramped smoothing according to parameter sensitivity; avoid abrupt discontinuities and zipper noise.
- Note/transport events remain event-driven and must not be delayed by unrelated UI work.
- Audio callback owns DSP runtime state. UI/settings code submits bounded parameter/state changes through the established event or snapshot mechanism.
- No heap allocation, blocking mutex, flash operation, unbounded loop, or logging in render.
- Reset/panic behavior must define how effect tails, filter state, and limiter state are cleared or allowed to decay.

## 7. Numeric and memory contract

- Choose and document one internal signal scale for each stage; document Q formats at API boundaries.
- Use wider accumulators for patch summing, FX send accumulation, FX returns, and master summing.
- Saturation is permitted only at explicitly documented boundaries and must increment/record diagnostics when it indicates unexpected overload.
- Establish worst-case bounds for four patch sums, four FX sends, four FX returns, feedback paths, compressor makeup, and master gain.
- Before implementing additional processors, produce a memory budget covering .pool, .bss, stack, delay/reverb buffers, filter state, and per-voice state against actual device linker regions.
- Host tests establish correctness; only device measurements establish callback CPU, worst-block time, and xrun qualification.

## 8. Required diagnostics

Expose or retain test access to per-patch pre/post-strip peak; dry-mix peak; each FX send-bus input peak and return peak; master pre-dynamics and post-dynamics peak; limiter gain reduction and ceiling events; intermediate saturation/overflow counters; callback CPU, xruns, and overload voice-shed count.

Meters must observe the signal without changing it. Diagnostic collection must remain bounded and must not format/log text from the audio callback.

## 9. Architecture acceptance tests

Before calling the mixer foundation complete, tests must demonstrate:

1. Four identical full-scale patch signals at 25% each, with default −12 dB master trim, produce the expected approximately −12 dBFS reference output.
2. One patch at 25% produces the same patch-strip gain regardless of whether one, two, three, or four patches are active.
3. A patch at level zero contributes exactly zero to dry output and every FX send.
4. Increasing one patch level changes only that patch's strip contribution (plus its routed FX contribution).
5. Four independent/default patch signals remain bounded under the documented gain budget.
6. All four FX returns active, including worst-case supported feedback, remain bounded without intermediate clipping.
7. Master trim and final limiter can be tested independently; bypassing the limiter does not reveal hidden intermediate clipping in valid reference conditions.
8. Limiter ceiling and gain reduction are deterministic and measured independently from master trim.
9. Parameter changes are smoothed and render output is chunk-size consistent.
10. Save/load and all supported migrations preserve user values; failed validation does not partially mutate runtime state.
11. Combined voice/sequencer/arp/FX/inserts/master stress remains bounded and allocation-free.
12. Static memory/stack budget and on-device CPU/xrun qualification pass their separate gates.

## 10. Source audit at architecture freeze

The current feature-branch implementation has several behaviors to reconcile before more DSP is added:

- The active-layer-count normalization has been removed. Each patch now receives the same fixed linear level regardless of how many other patches are active; the coherent-sum test verifies exact 4:1 pre-master scaling.
- The unsafe 127 default is repeated in settings_defaults(), instrument_factory_capture(), fm1lab_instrument_init(), fm1lab_patch_set_voice(), and the empty-bank record initializer in fm1lab_store_library.c. All factory/new-state entry points must agree on 31/127. Existing decoded/saved user records must retain their stored level; changing the default must not rewrite existing user data.
- The `APP_GAIN` caller contract is Q12 amplitude (0..4096). A fixed Q15 −12 dB master trim (8231) now precedes FX routing; the output path uses linear Q24-to-Q23 conversion and a separate −1 dBFS peak limiter with instantaneous attack and bounded release.
- The current shared FX send accumulator still folds patch samples to mono before the stereo returns. Per-patch stereo sends and pan-before-send remain outstanding; the present Q19 headroom work does not claim that routing requirement complete.
- FX send accumulators remain wide until the fixed master trim, then enter a Q19 internal FX domain (Q24 input shifted right five bits). The existing int32 delay/comb buffers retain their memory footprint while gaining 32× internal headroom. Test-only saturation counters verify no internal saturation under the four-patch reference/feedback condition.
- The current Phaser is a four-stage all-pass effect, while Chorus/Delay/Reverb still have simpler existing implementations. Their quality and parameter completeness remain separate work items.
- Current settings schema is V4. Instrument bank records already persist four patch levels/macros/enabled flags in their per-patch metadata, while the global settings record also duplicates levels/macros for compatibility and performance state. The bank record is the authoritative source after an Instrument is loaded; the settings copy is a compatibility/runtime mirror. Define explicit ownership before adding pan, inserts, sends, or master processor state. Do not expand the fixed 479-byte bank record or change its packed layout casually.

These are audit findings, not permission to make a rushed compatibility-breaking rewrite. Resolve them in order and keep existing supported migration tests intact.

## 11. Implementation sequence

1. **Architecture freeze:** settle signal units, state ownership, routing API, smoothing policy, memory budget, diagnostics, and migration ownership.
2. **Mixer foundation:** stable calibrated patch gain, wide accumulators, Q19 FX-bus headroom, −12 dB master trim, and a separate final limiter are implemented and under host regression. Per-patch pan/stereo sends and the complete routing contract remain outstanding.
3. **Patch inserts:** per-patch Amp/Drive and three-band EQ with independent state/bypass.
4. **Master dynamics:** compressor and independent peak limiter, with diagnostics and independent tests.
5. **FX refinement:** bring Chorus, Phaser, Delay, and Reverb to the shared parameter/routing contract; improve Reverb diffusion and document each processor's limits.
6. **Performance follow-through:** swing/ties, MIDI clock, arp extensions, and controller mapping after transport ownership is stable.
7. **Native UI integration:** connect existing physical controls/pages to the frozen parameter model. Preserve the established screen design; no visual redesign is part of this work.
8. **Qualification:** full host regression, memory/stack audit, native device build, then sustained hardware CPU/xrun and audio checks.

## 12. Explicit guardrails

- Do not merge to main without explicit user approval.
- Do not modify the uncommitted “Shipwreck v.01” visual direction.
- Do not substitute a browser mockup for native C firmware/emulator work.
- Do not claim device-level real-time or audio validation from host tests.
- Do not add new processors before their state, routing, numeric, memory, and test contracts are settled.


## 13. Follow-up source audit — real-time and memory gates

The following findings were confirmed against the current feature-branch source on 2026-11-05. They are additional blockers to the mixer foundation, not permission for a partial DSP rewrite.

### Render-path work and command budgeting

- `app_render()` calls `ui_events_drain()` and `midi_in()` before `fm6_render()`. The event queue is statically bounded to 63 pending entries, but a single drain can apply a full instrument reload (four patch rebuilds) or multiple voice edits in one audio callback. The queue is bounded in memory, not yet budgeted in worst-case callback time.
- `midi_in()` loops until `plat_midi_in()` reports empty and has no explicit per-callback packet budget in the app wrapper. Its worst-case work therefore depends on the platform queue behavior and needs a documented cap or a proven bounded platform contract.
- Preserve the single audio-owner model, but establish a maximum number/cost of commands applied per block. Expensive patch preparation should be built on the control side and published as a bounded snapshot/command; do not move storage, UI rendering, or allocation into the callback.
- The render function currently allocates a 4 × 32 `int64_t` patch-mix array (1,024 bytes) plus a 32-sample `int32_t` voice bus (128 bytes) in nested call scope, before other callees and platform ISR stack use. Measure the linked worst-case stack rather than assuming the documented 24,320-byte user stack is all available to this callback.

### Numeric/API items still unresolved

- `fm6_render()` smooths `master_gain_q12` as Q12 amplitude (0..4096), matching `fm1-base` `APP_GAIN(master)`. The output multiply shifts by 12; the fixed −12 dB trim is a separate Q15 stage (8231). The final Q24-to-Q23 conversion shifts by one, before the independent limiter.
- Per-patch and dry/send accumulators use `int64_t`. After master trim, sends enter the Q19 FX domain (`Q24 >> 5`) before the int32 feedback buffers; this provides 32× headroom without increasing the current 98,304-byte FX-buffer allocation. `fm6_sat_q23()` remains an explicit internal-state boundary and is counted in host tests. The final output is now linear Q24-to-Q23 conversion followed by a distinct peak limiter, not the former tanh soft clip.
- Patch level no longer divides by active-layer count. A measured Q15 calibration constant of 10,536 maps the pinned core's 13,045,204 Q24 reference peak at level 31 to 4,194,466 Q24 (25% of Q24 full scale); four coherent patches sum to 16,777,864 Q24, within 0.001% of unity.

### Current memory evidence and build target

- The active device target is the native `app/fm1.toml` build, which compiles `app/dsp/fm6.c`. The separate `firmware/fm1lab_audio_engine.c` path is not listed in that target and must not be mistaken for the live DSP implementation or patched as a substitute.
- Current `.pool` declarations in `app/dsp/fm6.c` total 98,304 bytes: 8,192 delay + 2,048 chorus + 2,048/4,096/8,192 comb samples, each 32-bit. The platform graphics canvas documented in the existing memory audit is another 69,120 bytes. These two known allocations alone total at least 167,424 bytes, before any other pool users.
- Therefore the older 69,384-byte `.pool` report in `docs/memory-budget.md` cannot describe the current source revision. It is historical evidence only until a fresh ELF is built from this exact branch head. Re-run the native device build and section report before allocating new buffers or declaring memory headroom.

### Required next audit actions

1. Trace the platform's `app_render` master argument from its actual call site and document its range, scale, and smoothing expectations.
2. Inspect the pinned FM6 core's sample format and every conversion into/out of the wrapper; publish a stage-by-stage numeric table.
3. Bound MIDI and command processing per audio block; measure patch-rebuild worst case and stack use.
4. Build this exact branch head with the pinned submodules/toolchain and capture current ELF `.text`, `.ram_text`, `.data`, `.bss`, `.pool`, and stack evidence.
5. Only then finalize accumulator widths, saturation points, fixed master trim, limiter interface, and the mixer API.


## 14. Measured gain foundation — 2026-11-06

The following values are implemented in `app/dsp/fm6.c` and covered by native host tests:

| Stage | Contract | Evidence |
|---|---|---|
| FM6 reference voice | Factory level 31, reference note 60 | Pinned core produced 13,045,204 Q24 before wrapper calibration |
| Patch calibration | Q15 multiplier 10,536 | Reference patch peak becomes 4,194,466 Q24 (approximately 0.25 Q24 full scale) |
| Four-patch dry sum | 64-bit coherent accumulation | Four identical patches measured 16,777,864 Q24 (approximately 1.0 Q24 full scale) |
| Default master trim | Q15 multiplier 8,231 (−12 dB) | Applied once to dry and send paths, before FX processing |
| FX internal domain | Q19, Q24 send shifted right 5 bits | 32× internal headroom while retaining the existing int32 delay/comb buffer allocation |
| Device output conversion | Linear Q24 to signed Q23 | No tanh soft-clip stage in the output path |
| Final peak limiter | 7,470,000 Q23 (approximately −1.01 dBFS) | Instantaneous attack, approximately 5.8 ms release, shared stereo gain envelope |

The coherent four-patch test asserts a unity pre-trim dry sum and an output peak between 24% and 26% of Q23 full scale at default master gain. A separate four-bus feedback test asserts zero internal FX saturation at the reference patch level. A full-level combined stress test asserts the final output never exceeds the limiter ceiling and that gain reduction is exercised.

This is a host-calibrated signal reference, not a universal loudness guarantee for arbitrary patches, notes, or resonant effect settings. The limiter is a final safety stage; it does not excuse intermediate saturation. The test-only probes are excluded from production builds. Hardware CPU/xrun, clipping behavior, and listening qualification remain outstanding.
