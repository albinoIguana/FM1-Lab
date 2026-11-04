# Native two-lane sequencer adapter

## Scope

FM1-Lab exposes two sequencer lanes while the M-VAVE project format retains eight tracks. The adapter binds two explicitly selected tracks; it does not move, clear, or rewrite the other six project rows.

Only tracks whose role is `GROOVE_TRACK_NORMAL` and source type is `GROOVE_DX7` can be bound. This keeps the four-patch FM Instrument workflow from accidentally attaching to a drum, sampler, or other synth track.

## Playback isolation

The upstream sequencer normally advances all eight tracks. FM1-Lab adds a runtime-only enable mask in the sequencer engine:

- Binding two lanes enables only those two underlying tracks for sequencer note events.
- Disabling a track releases its currently held notes and restores its parameter locks.
- The six other tracks' note data, lengths, presets, roles, and project storage remain unchanged.
- Unbinding restores all eight tracks to normal sequencer playback.
- The enable mask is not serialized into project data; a project load resets the engine to its normal all-enabled state. The FM1-Lab session must bind again after loading a project.

The mask is deliberately separate from mute/solo and track roles. It is a temporary execution scope for the two-lane FM1-Lab sequencer.

## Native field mapping

| FM1-Lab field | M-VAVE engine representation |
|---|---|
| Active | Step velocity is non-zero |
| Pitch | `groove_step.note` |
| Velocity | `groove_step.velocity` |
| Gate | `groove_step.drums` bits 5–8, accessed through setting 1 |
| Tie | `groove_step.drums` bit 12, accessed through setting 4 |
| Alternate tuning | `groove_step.drums` bit 13 (`GROOVE_STEP_ALT`) |
| Loop length | 16, 32, 48, or 64 steps |
| Tempo | Shared project tempo, 30–300 BPM |
| Swing | Shared sequence option, 0–50 |

Gate value zero is the native 50% default; values 1–14 represent shorter gates and 15 is a full step. A zero step velocity is a rest. Alternate tuning is not slide or glide.

## Adapter API

`firmware/fm1lab_sequencer.c` provides bind/unbind, state read, tempo/swing/length editing, step editing, lane selection, patch assignment from the four-patch Instrument, patch level and mute application, and transport start/stop. Patch assignment renders the selected Instrument patch through the macro renderer, imports the resulting DX7 voice into the bound track, then applies its level and enabled state. All functions are control-task operations. The adapter does not perform flash I/O; the existing project system remains responsible for persistence and undo.

## Native screen integration

The build-time screen patch adds a dedicated two-lane page entered with the physical SEQ button from the Instrument home screen. SELECT moves the 64-step cursor; PRESETS selects lane A/B; ALGORITHM selects one of the four Instrument patches for that lane; KNOB 1 enables/disables the selected step, KNOB 2 changes pitch, KNOB 3 changes velocity, and KNOB 4 changes gate. PLAY toggles the native sequencer transport. The display shows a 16-step window of the selected lane position, with a fixed 64-step timeline and the native loop length.

The sequencer page binds the first two normal DX7 tracks in the current project. Binding changes only the runtime playback mask; it does not rewrite other six tracks. Explicit step edits change the selected steps on the two bound tracks. Selecting an Instrument patch for a lane imports that patch into the corresponding project track, as expected for a sequencer track assignment. Leaving the sequencer stops playback and restores the normal all-track playback mask.

## Remaining integration

The native screen patch and sequencer adapter are included in both the simulator build and the Windows device-build staging script. CI compiles the official M-VAVE browser engine and exercises the host-side C adapter tests. This is not yet a verified JieLi device firmware build or a flashable hardware image. The device build still requires the matching JieLi toolchain, and actual FM1 hardware validation remains outstanding. Browser editing remains a development surface, not the physical firmware deliverable.
