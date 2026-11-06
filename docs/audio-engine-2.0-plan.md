# FM1-Lab Audio Engine 2.0 implementation plan

Status: calibrated mixer, pan-aware stereo sends/returns, per-Instrument Amp/Drive + three-band EQ, stereo-linked master compressor, and their compact physical-control pages are implemented on the feature branch. The binding architecture contract is [audio-engine-2.0-architecture.md](audio-engine-2.0-architecture.md); it supersedes any conflicting assumptions in this working plan. Items below are not complete unless explicitly marked.

## Implementation checkpoint — Phaser bus

- Implemented in the native FM6 render path as a fourth parallel send/return bus, not a Chorus alias.
- Four cascaded first-order all-pass stages per channel; dry/wet summing creates the phase-cancellation notches.
- Independent LFO phase accumulator (approximately 0.05–4.05 Hz), 90-degree stereo phase offset, depth, bounded feedback/resonance, and wet return.
- Per-patch Phaser send is independent of Chorus, Delay, and Reverb sends.
- Performance-state schema expanded to four buses and master compressor state; settings V1/V2/V3/V4 migrations are supported into the current V5 record.
- Host tests cover audible signal change, stereo spread, per-patch send isolation, combined four-bus stress, state round-trip, depth validation, and render chunk consistency.
- Wired to a dedicated native PHASER FX page with wet, rate, depth and resonance controls. A separate SEND ROUTING page exposes the four independent per-patch bus sends. Both SELECT and the FX button cycle the four processor pages plus routing. Device CPU/xrun qualification remains outstanding.

## Implementation checkpoint — calibrated mixer and master safety

- Measured the pinned FM6 core's reference voice at 13,045,204 Q24 for factory level 31; fixed Q15 calibration 10,536 yields 4,194,466 Q24 per reference patch.
- Four coherent reference patches sum to 16,777,864 Q24 (approximately unity) without active-layer-count normalization.
- Added the independent Q15 −12 dB master trim (8231) ahead of dry/FX routing.
- Moved the shared FX buses into a Q19 internal domain (Q24 sends shifted right five bits), reserving 32× feedback headroom. Pan-aware stereo routing now uses independent left/right delay, chorus and comb buffers.
- Replaced the tanh output clip with linear Q24-to-Q23 conversion and a distinct stereo-linked peak limiter at 7,470,000 Q23 (approximately −1 dBFS), instantaneous attack and approximately 5.8 ms release.
- Native tests now measure the coherent reference, assert the default output is approximately −12 dBFS, verify no internal FX saturation in the reference feedback case, and exercise the final limiter under full-level stress.
- Per-patch pan is persisted in bank V2 using reserved bits without expanding the 479-byte record; V1 banks migrate to center. The dry strip uses smoothed constant-voltage balance pan, with hard-left/right/center and persistence tests.
- Latest native device build passes. ELF audit: `.data` 2,968 B, `.bss` 44,696 B, `.ram_text` 3,556 B, `.pool` 275,264 B, `.text` 181,976 B, `.noinit` 1,836 B; app.bin 188,500 B. Pool spare is 68,800 B (60,608 B after the mandatory 8 KiB reserve); RAM `.data + .bss` spare is 50,632 B.
- Per-patch stereo sends/pan-before-send are implemented and covered by host probes. Master compressor and Amp/EQ/compressor physical controls are implemented. Hardware CPU/xrun/listening qualification remains incomplete.

## Architecture gate — insert and master state ownership settled

Per-Instrument inserts use a CRC-protected sidecar journal, while master compressor parameters are Performance/settings-owned with an explicit V4-to-current migration. The fixed 479-byte Instrument bank record is unchanged. The remaining architecture work is real-time command budgeting, worst-case stack measurement, FX quality refinement and hardware qualification. Do not treat the completed peak limiter as completion of musical compression.

## Goals

Complete the native FM1 audio path before spending time on visual polish. Every visible control must affect real DSP state, every persistent control must survive save/load, and all processors must be covered by deterministic host tests. The browser emulator is a development aid; hardware real-time qualification remains a separate release gate.

## Signal-flow contract

```text
Four independent FM6 patch voice pools
  -> per-patch level
  -> per-patch INSERTS: Amp/Drive -> 3-band EQ
  -> per-patch pan
  -> patch sum / headroom management
  -> parallel SEND buses: Chorus | Phaser | Delay | Reverb
  -> wet/dry return mix
  -> master compressor / peak limiter
  -> master gain -> stereo 24-bit output
```

- Amp/Drive and EQ are inserts, independently enabled and controlled per patch. One layer can be distorted or EQ'd without changing the others.
- Chorus, Phaser, Delay and Reverb are parallel send/return effects with independent sends from each patch. Phaser is not implemented as a chorus-delay alias.
- Compressor is a master dynamics processor; the final peak limiter is a separate safety stage and must not be mistaken for musical compression.
- Pan is per patch and applied before the shared FX sends so spatial placement is coherent. The stereo effects may widen/alter that image.
- All processing is fixed-memory, allocation-free and bounded in the audio callback. No flash, UI, logging, or blocking calls in DSP.

## Processor requirements

### Shared send effects
1. Chorus: retain stereo modulation, but expose independent rate, depth, feedback and wet controls. Do not overload one generic time parameter to stand for both rate and depth.
2. Phaser: four or six all-pass stages per channel, low-frequency oscillator with rate and depth, bounded resonance/feedback, stereo phase offset, wet control. Include feedback stability tests and a documented frequency range.
3. Delay: tempo-free time control first; feedback, wet and stereo mode/offset. Later add tempo sync after external MIDI clock is reliable. Prevent runaway feedback.
4. Reverb: replace the current three-comb approximation with a small, documented diffusion network (input diffusion plus multiple decorrelated delay lines), with decay, damping/tone, pre-delay and wet controls. Keep buffer sizes inside the existing .pool reserve.

### Per-patch inserts
5. Amp/Drive: implemented with bounded pre-gain, monotonic soft-knee saturation, tone and output trim. State and bypass are independent per patch.
6. EQ: implemented as three fixed-point peaking bands with frequency, gain, Q, global bypass and independent band enable. State is independent per patch.

### Master
7. Compressor: implemented with threshold, ratio, attack, release and makeup; stereo-linked detector and smoothed envelope. The final peak limiter remains separate.

## Controls and persistence

- Use named effect and parameter enums; remove magic numeric indices from UI and DSP.
- Four shared FX buses and their per-patch sends remain in the versioned Performance/settings record. Master compressor parameters are also Performance/settings-owned; prior V4 settings migrate with compressor bypassed.
- Amp/Drive and EQ are Instrument-owned in a separate versioned sidecar journal. The original 479-byte bank records are unchanged; V1 sidecars migrate to V2 and pending two-object saves are resolved by bank CRC plus generation.
- All records are CRC-protected and range-validated before mutation; host tests cover legacy bank migration, settings V1–V4 migration and sidecar crash recovery.
- One effect per page remains the UI navigation pattern. Every page must expose the four most musically useful controls for that processor, with live DSP-backed values and an explicit bypass state. Do not force the same four generic parameter names onto different processors.
- FX knob assignments are provisional until each processor has real independent DSP parameters. In particular, chorus rate/depth and reverb damping/pre-delay must be distinct controls.

## Sequencing / performance follow-through

- Add swing and tie behavior to the two native lanes, with deterministic clock tests.
- Add MIDI clock receive/start/stop/continue and a stable fallback to internal clock.
- Expand arp range, octave span, pattern direction, latch and clock division only after transport ownership is clarified.
- Complete MIDI CC learn/mapping, sustain and expression behavior without doing UI work in the audio interrupt.

## Verification gates

For each processor and each routing edge:
- Parameter bounds and invalid-input rejection.
- Bypass transparency (bit-identical where mathematically possible).
- Wet/dry behavior, finite bounded output, DC/denormal protection and tail behavior.
- Per-patch insert isolation and per-patch send isolation.
- Repeated enable/disable, parameter changes while sounding, panic and reset.
- Combined 32-voice + dual sequencer + 16-note arp + all inserts/sends/master dynamics stress.
- Render chunk equivalence at legal FM6 32-frame quanta.
- Static memory/stack audit and linker-region budget after adding buffers.
- Device CPU percentage, worst block cost, xrun and overload-shed deltas under sustained hardware stress. Host tests do not substitute for this.

## Delivery order

1. Freeze enums, routing topology, parameter semantics, settings version/migration and DSP ownership. The four stereo send buses and pan-before-send topology are implemented; insert/master persistence ownership remains to be frozen.
2. Implement the processor APIs and parameter state without UI assumptions.
3. Implement Phaser and Amp/Drive, then EQ and master Compressor/Limiter.
4. Refactor Chorus, Delay and Reverb to expose meaningful independent parameters and improve quality.
5. Integrate per-patch pan, inserts, shared sends and master processing into the render path.
6. Add exhaustive host tests, migration tests, combined stress tests and update the memory report.
7. Connect physical controls/pages after the DSP contracts stabilize.
8. Flash and qualify on hardware; do not label the build release-ready before this gate.

## Explicit non-goals for this milestone

- No visual redesign while DSP/routing contracts are changing.
- No promise of 32-voice real-time performance until measured on the FM1.
- No additional novelty effects (flanger, bitcrusher, rotary) until the core rack is stable.
