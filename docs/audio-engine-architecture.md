# Native four-patch audio engine

## Current architecture

FM1-Lab adds four independently controlled six-operator synth instances by reusing the fixed-point DX7 engine already linked in the pinned M-VAVE FM1 SDK. It does not compile or package a second DX7 DSP core.

- Four independent synth instances represent the four patches in the selected Instrument.
- Each patch has four note voices in this first hardware pass (16 six-operator voices total).
- A local keyboard MIDI note is sent to every enabled patch while the Instrument home, patch editor, or sequencer screen is active.
- Patch enable and level changes, macro-rendered voice updates, pitch bend, modulation, sustain, and other channel MIDI messages are routed to the independent instances.
- The existing project-track engine continues rendering normally. Its output is mixed with the Instrument output; local keyboard notes are withheld from project tracks only while an Instrument screen is active.
- The two-track sequencer UI remains backed by its existing two project FM tracks in this pass. Routing sequencer lanes directly into selected Instrument patch pools is a separate follow-up.

## Device audio path

The build-time patch scripts/patch-m-vave-audio-mixer.mjs modifies the official device audio callback:

1. fm1_audio_start initializes the four-patch engine before the DMA audio stream starts.
2. fm1_audio_irq renders the normal M-VAVE project engine.
3. fm1lab_audio_mix renders the four Instrument layers using the SDK's existing `synth_dx7_*` API and adds their mono Q24 signal to the existing stereo Q23 buffer.
4. The official fm1_output_mix continues to handle the final device output path.

The Instrument wrapper allocates synth instances during initialization, not in the audio interrupt. Control and MIDI changes enter a bounded single-producer/single-consumer command queue; the audio callback applies queued changes between render blocks. The render path uses fixed-point layer gain and 32-bit saturating addition.

## Engine selection and source isolation

The device build reuses the six-operator fixed-point DX7 synth already present in the pinned FM1 SDK engine archive. The SDK exports the required create, destroy, reset, MIDI, LFO, render, all-notes-off and voice-count entry points. The device link map confirmed these stock symbols were already present.

The previous implementation staged a second copy of the same M-VAVE DX7 sources under an `fm1lab_` namespace. That created duplicate DSP code and duplicate lookup tables alongside the stock engine. The device staging script now copies only the API header and removes stale generated copies. It does not compile another engine.

The host C test compiles the pinned upstream source implementation of this same API so that the four-layer wrapper can be tested without proprietary SDK binaries. The separate fm1-base host target may still use Felucca's FM6 reference core; that is not included in the actual M-VAVE device firmware.

The pinned M-VAVE repository revision is `4c7496daac04a95ff10d1afdcc8f9ead2dbabc41`.

## Validation status

- Native C tests cover four-patch initialization, note fan-out, per-patch enable/level, MIDI routing, voice counts, mixing, and queue-overflow recovery against the pinned source implementation of the SDK API.
- The official M-VAVE browser simulator build and native API check pass.
- The Windows CI job builds the actual device with the exact JieLi toolchain and validates the official package layout.
- Physical FM1 playback, CPU headroom, heap use, and flash/recovery behavior still require device validation. A successful browser build is not a substitute for that test.
