# Felucca foundation and UI direction

## Why Felucca

Use Felucca as the firmware and synthesis foundation, not merely as a firmware image to boot in the generic emulator. Its browser target compiles the actual C application to WebAssembly against a simulated FM-1 host. The exported host API is intended for custom frontends.

Pinned source: https://github.com/hugelton/Felucca/tree/743551712c36b3984392eea1dbe13a309907e17a

Felucca is GPL-3.0-only. Its engine library is MIT and its board layer is MPL-2.0. Preserve notices and make corresponding source available with distributed builds.

## What we inherit

- Four independent tracks/parts.
- A six-operator FM engine (FM6) with 32 algorithms.
- A native, tested topology graph for all 32 FM6 algorithms (`ui_graph.c` / `fm6_core.c`), including carrier/modulator routes and feedback.
- Dedicated FM6 operator, envelope and secondary operator edit pages (`ui_fm6op.c`).
- Existing envelopes, LFO, modulation matrix, sequencer, arpeggiator, mixer and effects.
- A parameter description system and a documented editor protocol.
- A browser host API for audio rendering, keys, buttons, encoders, MIDI, display, LEDs and flash.

The four-track structure is a strong candidate for the four-patch Instrument model, but it is not yet declared equivalent. We must prove the sound-only save/load semantics and ensure the Instrument does not accidentally become a full sequencer project.

## Interaction language

The accepted boot animation stays. After boot, the workspace should read as an instrument panel:

- hardware-first hierarchy and compact parameter pages;
- visible physical-control metaphors, clear encoder assignments and direct feedback;
- the FM-1's pixel-screen/LED character rather than a generic web dashboard;
- no decorative visualizer taking space away from the actual controls.

Effects use the Clipper-inspired workflow, not Clipper code: select an effect, navigate its pages, turn the four primary controls and hear the result immediately. Bypass and page navigation stay obvious; advanced parameters are available without crowding the first page.

FM editing already has a strong firmware-native foundation: Felucca draws the 32 FM6 algorithms from its actual routing table, labels carriers and modulators, marks feedback, and provides operator/envelope edit pages. The custom layer should expose and extend this tested model—not invent a parallel topology. Four main controls can surface role-relevant macros for the selected algorithm; the deeper operator view retains raw ratios, levels, envelopes and feedback.

Each Instrument contains four independent FM patches. The patch owns its internal operator graph and routing. Instrument-level controls select, mute/solo and mix patches. Stacked, split and velocity-zone behavior is added only after the four-part voice and MIDI behavior is verified against Felucca.

## Integration order

1. Build and publish Felucca's browser runtime alongside the generic emulator.
2. Add a deliberate worklet message bridge for the FM1-Lab frontend; do not scrape WebAssembly memory or couple to private exports.
3. Replace the independent local DSP with calls into Felucca's runtime.
4. Implement the four-patch Instrument model against Felucca's four parts.
5. Add algorithm-aware FM editing and the Clipper-inspired FX pages.
6. Validate sound, flash persistence and touch/encoder interaction in the generic emulator and on hardware when available.
