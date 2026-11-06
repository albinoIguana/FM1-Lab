# FM1-Lab Audio Engine 2.0 implementation plan

Status: Phaser fourth bus, calibrated gain/headroom foundation, and persistent per-patch dry pan are implemented on the feature branch. The binding architecture contract is [audio-engine-2.0-architecture.md](audio-engine-2.0-architecture.md); it supersedes any conflicting assumptions in this working plan. Items below are not complete unless explicitly marked.

## Implementation checkpoint — Phaser bus

- Implemented in the native FM6 render path as a fourth parallel send/return bus, not a Chorus alias.
- Four cascaded first-order all-pass stages per channel; dry/wet summing creates the phase-cancellation notches.
- Independent LFO phase accumulator (approximately 0.05–4.05 Hz), 90-degree stereo phase offset, depth, bounded feedback/resonance, and wet return.
- Per-patch Phaser send is independent of Chorus, Delay, and Reverb sends.
- Performance-state schema expanded to four buses with a versioned settings-record migration from V3; legacy V1/V2/V3 records remain supported.
- Host tests cover audible signal change, stereo spread, per-patch send isolation, combined four-bus stress, state round-trip, depth validation, and render chunk consistency.
- Wired to a dedicated native PHASER FX page with wet, rate, depth and resonance controls. A separate SEND ROUTING page exposes the four independent per-patch bus sends. Both SELECT and the FX button cycle the four processor pages plus routing. Device CPU/xrun qualification remains outstanding.

## Implementation checkpoint — calibrated mixer and master safety

- Measured the pinned FM6 core's reference voice at 13,045,204 Q24 for factory level 31; fixed Q15 calibration 10,536 yields 4,194,466 Q24 per reference patch.
- Four coherent reference patches sum to 16,777,864 Q24 (approximately unity) without active-layer-count normalization.
- Added the independent Q15 −12 dB master trim (8231) ahead of dry/FX routing.
- Moved the shared FX buses into a Q19 internal domain (Q24 sends shifted right five bits), preserving existing int32 delay/comb memory while reserving 32× feedback headroom.
- Replaced the tanh output clip with linear Q24-to-Q23 conversion and a distinct stereo-linked peak limiter at 7,470,000 Q23 (approximately −1 dBFS), instantaneous attack and approximately 5.8 ms release.
- Native tests now measure the coherent reference, assert the default output is approximately −12 dBFS, verify no internal FX saturation in the reference feedback case, and exercise the final limiter under full-level stress.
- Per-patch pan is persisted in bank V2 using reserved bits without expanding the 479-byte record; V1 banks migrate to center. The dry strip uses smoothed constant-voltage balance pan, with hard-left/right/center and persistence tests.
- Latest native device build passes. ELF audit: `.data` 2,968 B, `.bss` 44,108 B, `.ram_text` 3,556 B, `.pool` 171,528 B, `.text` 167,960 B, `.noinit` 1,836 B; app.bin 174,484 B. Pool spare is 172,536 B; RAM `.data + .bss` spare is 51,228 B.
- Per-patch stereo sends/pan-before-send, master compressor, pan UI control, and device CPU/xrun qualification are not complete.

## Architecture gate — before inserts and further DSP

The gain foundation and per-patch dry pan/state migration are now in place, but the architecture contract still gates the next processors. Freeze stereo send/return ownership, instrument/performance/device state boundaries for sends/inserts, memory/stack budget, and remaining saturation boundaries before adding Amp/Drive or EQ. Do not treat the current mono send accumulation or the completed peak limiter as completion of the full mixer or master-dynamics architecture.

## Goals

Complete the native FM1 audio path before spending time on visual polish. Every visible control must affect real DSP state, every persistent control must survive save/load, and all processors must be covered by deterministic host tests. The browser emulator is a development aid; hardware real-time qualification remains a separate release gate.

## Signal-flow contract

```text
Four independent FM6 patch voice pools
  -> per-patch level and pan
  -> per-patch INSERTS: Amp/Drive -> 3-band EQ
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
5. Amp/Drive: bounded pre-gain, drive/character, tone and output trim; use a smooth, monotonic saturation curve with DC protection. Per-patch state and bypass must be independent.
6. EQ: low, mid and high bands with stable fixed-point filters, bounded gain and no denormal/overflow issues. Per-patch state and bypass must be independent.

### Master
7. Compressor: threshold, ratio/amount, attack, release and makeup gain; use a smoothed detector and gain envelope. Keep the final peak limiter separate, deterministic and non-oscillatory.

## Controls and persistence

- Use named effect and parameter enums; remove magic numeric indices from UI and DSP.
- Replace the current three-effect arrays with an explicitly versioned performance-state layout for four shared buses plus per-patch insert state and master dynamics.
- Preserve V1/V2/current settings imports. New records must be CRC-protected, range-validated before mutation, and migrated without partially applying invalid records.
- Add pan and all new insert/master parameters to the same save/load transaction as tempo, sequencer, arp and FX.
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

1. Freeze enums, routing topology, parameter semantics, settings version/migration and DSP ownership.
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
