# Future sequencer concept: Slink-inspired motion

Status: design note only. Not part of the current cleanup or release gate.

## Intent

Explore an organic, evolving pattern-generation mode inside the existing single-track
sequencer. The reference is the *behavior* of Hypnus Records' Slink MIDI: two independently
moving wave fields are sampled against a clock and translated into musical events. This
is not a port of Slink, and it is not the Slink Filter audio effect.

## Proposed FM1-Lab interpretation

- One deterministic, fixed-point motion engine, evaluated at sequencer/control rate.
- Two independent 16-node wave fields: one primarily influences pitch, the other trigger
  probability/velocity. Their phase relationship creates evolving, non-repeating-feeling
  patterns without requiring random note selection on every tick.
- A clock-sampled output maps to pitch, velocity, and note duration for the existing
  single sequencer lane.
- Musical controls to explore: rate / tempo sync, phase, reverse, freeze, multiply,
  ripple, offset, threshold, swing, per-node clock division, and note-length range.
- Trigger behavior options: retrigger, once-until-reset, and threshold-crossing.
- A generated pattern should be capturable into ordinary sequencer events, editable in
  the piano roll, and retainable as a normal saved pattern. Generation must never
  overwrite a user's pattern without an explicit action.
- Provide a deterministic seed / reset point so a performer can return to a useful
  variation. Freeze should stop evolution without changing the currently held pattern.

## Scope and constraints

- This is a sequencer/generative-control feature, not a fifth synth engine or a fifth FX.
- Do not add a 32-band filter bank to the audio path. Hypnus' Slink Filter is a separate
  spectral audio processor; the sequencer inspiration is Slink MIDI's note-generation
  behavior.
- Keep the existing ARP and Maelstrom behavior independent and intact.
- Keep the feature off the real-time audio render path. No heap allocation, floating-point
  trigonometry per sample, or unbounded work in the audio callback.
- First prototype should be host-tested for repeatability, event bounds, timing, and
  memory cost before any FM1 hardware work.

## Reference

Hypnus / Ableton's Slink Devices description identifies separate Slink Filter, Slink LFO,
and Slink MIDI devices. Slink MIDI samples two animated wave fields to derive pitch,
velocity, and note length. Slink Filter instead animates a bank of bandpass-filter
amplitudes. FM1-Lab should borrow the generative sequencing concept only.
