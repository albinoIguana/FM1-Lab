# Native four-patch audio engine

## Current architecture

FM1-Lab adds a source-built six-operator engine alongside the existing M-VAVE project engine. It does not replace the stock engine or reserve project tracks.

- Four independent DX7 synth instances represent the four patches in the selected Instrument.
- Each patch has four note voices in this first hardware pass (16 six-operator voices total).
- A local keyboard MIDI note is sent to every enabled patch while the Instrument home, patch editor, or sequencer screen is active.
- Patch enable and level changes, macro-rendered voice updates, pitch bend, modulation, sustain, and other channel MIDI messages are routed to the independent engines.
- The existing project-track engine continues rendering normally. Its output is mixed with the Instrument output; local keyboard notes are withheld from project tracks only while an Instrument screen is active.
- The two-track sequencer UI remains backed by its existing two project FM tracks in this pass. Routing sequencer lanes directly into selected Instrument patch pools is a separate follow-up.

## Device audio path

The build-time patch scripts/patch-m-vave-audio-mixer.mjs modifies the official device audio callback:

1. fm1_audio_start initializes the four-patch engine before the DMA audio stream starts.
2. fm1_audio_irq renders the normal M-VAVE project engine.
3. fm1lab_audio_mix renders the four independent FM6 layers and adds their mono Q24 signal to the existing stereo Q23 buffer.
4. The official fm1_output_mix continues to handle the final device output path.

The Instrument engine allocates synth instances during initialization, not in the audio interrupt. Control and MIDI changes enter a bounded single-producer/single-consumer command queue; the audio callback applies queued changes between render blocks. The render path uses fixed-point layer gain and 32-bit saturating addition.

## Source isolation

The M-VAVE device SDK links a fixed native engine archive. FM1-Lab does not assume that archive exposes the browser engine's private synth-construction API. Instead, scripts/patch-m-vave-device-dx7-core.mjs stages the source-available DX7 implementation from the pinned M-VAVE repository into app/fm1lab_dx7 and gives its symbols an fm1lab_ namespace. This allows the extra engines to coexist with the stock archive without symbol collisions.

The source files retain their upstream SPDX headers. The upstream repository remains pinned at commit 4c7496daac04a95ff10d1afdcc8f9ead2dbabc41.

## Validation status

- Native C tests cover four-patch initialization, note fan-out, per-patch enable/level, MIDI routing, voice counts, mixing, and queue-overflow recovery.
- The official M-VAVE browser simulator build and native API check pass.
- A Windows CI job now attempts the exact JieLi device toolchain build and packages the resulting .fwsc image.
- Physical FM1 playback, CPU headroom, heap use, and flash/recovery behavior still require device validation. A successful browser build is not a substitute for that test.
