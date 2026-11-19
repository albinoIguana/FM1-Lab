# FM1-Lab modular runtime architecture

## Non-negotiable product invariants

- Exactly four physical Instrument slots, in fixed order: FM6, wavetable, PSG,
  subtractive.
- Four engines remain independently playable at once.
- Preserve ARP and Maelstrom.
- One live sequencer track.
- Retain Chorus, Delay, Reverb, and Phaser.
- Never conceal resource problems by reducing polyphony, levels, or engine
  availability.
- Keep saved-record compatibility at the persistence boundary.

## Module ownership

| Module | Owns | Must not own |
|---|---|---|
| Platform adapters | Hardware MIDI, controls, display, audio callback, clock | Musical policy, DSP state, save layouts |
| Application coordinator | Lifecycle and routing between services | Synthesis algorithms, codecs, effect buffers |
| Instrument manager | Four live patch slots, engine mapping, patch controls | Sequencer events, voice internals, flash layout |
| Performance engine | ARP, Maelstrom, transport, one sequencer lane | Patch serialization, effect buffers |
| Synthesis host | Voice allocation and four-engine dispatch | UI, storage, sequencer persistence |
| Effects bus | Chorus, Delay, Reverb, Phaser, sends/returns | Patch ownership, sequencer, storage |
| Persistence service | Versioned records, CRC, defaults, migrations | Live runtime allocation |

## Runtime contracts

- Runtime capacities are defined once in `fm1lab_dsp_config.h`.
- Public headers expose contracts and value types, not mutable implementation
  arrays.
- Each mutable runtime state has one owner; other modules use commands or
  immutable snapshots.
- Audio-thread operations are bounded, allocation-free, and non-blocking.
- UI and storage work never execute in the audio callback.
- Legacy two-bank records are decoded by persistence and mapped into the
  current single-track snapshot. Legacy record shape must not dictate live
  runtime allocation.
- Structural refactors must not change musical behavior in the same commit.

## Refactor sequence

1. Record baseline tests, linker regions, callback timing, and all dual-bank
   state references.
2. Centralize capacities and physical slot identity. This step is deliberately
   behavior-preserving.
3. Isolate versioned persistence codecs and migration fixtures.
4. Define the four-slot synthesis-host contract; reduce runtime patch/voice
   allocation only after all render, note, FX-send, and telemetry paths use it.
5. Isolate the single-lane performance runtime while retaining ARP and
   Maelstrom.
6. Isolate the four-effect bus and verify all 16 FX combinations with all four
   engines audible.
7. Extract application/platform responsibilities incrementally.
8. Require native tests, firmware tests, emulator, device build, packaging,
   checksums, diff review, and physical hardware verification before flashing.

Each stage is a small reviewable commit. Do not rewrite the synthesis core or
combine module extraction with sound-design changes.
