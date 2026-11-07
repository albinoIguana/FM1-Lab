# FM1-Lab sound philosophy

## North star

FM1-Lab is a playable harmonic instrument, not a six-operator parameter spreadsheet. Its sound design should make the relationship between pitch, spectrum and time understandable by ear and immediate to control.

The instrument must be capable of heavy, grimy, abrasive sounds without sacrificing musical pitch, phrasing or expressive movement. "Musical" and "aggressive" are not opposing presets; they are dimensions that can be combined.

## Principles drawn from the references

1. **Ratios establish identity.** Carrier/modulator frequency relationships determine whether partials reinforce a clear harmonic pitch or spread into inharmonic, metallic and noisy structures. Preserve musically useful ratio relationships as first-class design choices.
2. **Modulation depth is spectral energy.** Raising modulator output increases modulation index and expands the audible sideband structure. Treat this as a core timbre control, not an arbitrary brightness filter.
3. **Envelopes make timbre perform.** Modulator envelopes determine how harmonic complexity changes through the note; carrier envelopes determine amplitude behavior. A patch should be judged over its whole note, not just its sustain spectrum.
4. **Feedback is a distinct kind of aggression.** It changes the spectrum of its own operator and can add upper-harmonic bite faster and less predictably than ordinary modulator-level changes. Keep it musically controllable and never use it as a substitute for all other timbre controls.
5. **Algorithm topology matters.** A macro must know which operators are carriers and which are modulators in the current algorithm. It must not blindly treat all six output levels as interchangeable.

## Four macro directions

These are musical roles, not fixed parameter aliases. Their implementation must be topology-aware and bounded.

- **BRIGHTNESS** — increase/decrease harmonic energy from modulators, prioritizing audible spectral change while preserving the carrier's pitch foundation.
- **BODY** — strengthen the fundamental and lower/central harmonic weight through carrier balance and appropriate modulator contribution; avoid merely turning up total volume.
- **MOTION** — make harmonic structure evolve over time through modulator envelope depth and selected time behavior. It should create movement, not simply wobble pitch by default.
- **DIRT** — add controlled instability and spectral aggression through feedback and carefully bounded inharmonic/modulation choices. Keep a usable center and avoid uncontrolled parameter jumps.

Neutral macro value is 50. Macro movement must be non-destructive: the stored source voice remains unchanged, and the rendered voice is derived from it. Macro changes should be smooth, predictable in direction, and algorithm-aware.

## Sound families to develop

1. **Harmonic foundations:** basses, keys, plucks, mallets and sustained tones with clear pitch and stable intervals.
2. **Animated harmonics:** evolving brass, strings, choirs, pads and sequences whose modulator envelopes create movement.
3. **Metal and machinery:** bells, gongs, struck metal, clangorous percussion and resonant drones using deliberate inharmonic ratios.
4. **Heavy / dirty:** growling basses, tearing leads, industrial stabs and distorted-sounding textures using high modulation indices and feedback.
5. **Hybrid instruments:** four coordinated patches with distinct roles (foundation, bite, motion, impact), designed to be played together as one instrument.

## Four-patch Instrument philosophy

A four-patch Instrument is a small ensemble under one set of hands. The four patches should have complementary musical jobs, not simply be four unrelated presets. Each patch can be enabled/muted and level-balanced independently. Their note behavior, tuning and envelopes must work together.

## UX consequences

- Show algorithm topology and carrier/modulator roles clearly.
- Make ratio relationships and modulator depth easy to hear and change.
- Expose macro behavior in context of the current algorithm.
- Preserve access to raw six-operator editing for deep sound design.
- Provide immediate auditioning and compare-before/after behavior.
- Never label a control "brightness" if it only changes unrelated LFO or amplitude behavior.

## Reference reading

- Anders Øland and Roger B. Dannenberg, Carnegie Mellon University, *FM Synthesis*: modulation index, sidebands, harmonicity and inharmonicity.
- Manny Fernandez, YamahaSynth, *Manny's Modulation Manifesto: Intro to FM Synthesis*: practical ratio, level, envelope and feedback programming.

This document is a design constraint for future macro, preset and editor implementation. It does not claim the current macro code already meets these goals.
