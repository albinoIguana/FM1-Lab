# Native two-lane sequencer adapter

## Scope

FM1-Lab exposes two sequencer lanes while the M-VAVE project format retains eight tracks. The adapter binds two explicitly selected tracks. Playback isolation uses the pinned official track-role API; see the side effects and undo-history caveat below.

Only tracks whose role is `GROOVE_TRACK_NORMAL` and source type is `GROOVE_DX7` can be bound. This keeps the four-patch FM Instrument workflow from accidentally attaching to a drum, sampler, or other synth track.

## Playback isolation

The pinned official API does not expose the former test-only `groove_sequence_track_enable()` mask. The adapter uses the supported `groove_track_role_set()` API instead:

- The two selected lanes remain `GROOVE_TRACK_NORMAL`.
- Other tracks that were `NORMAL` are temporarily changed to `GROOVE_TRACK_OFF`.
- Tracks already `PLAY` or `OFF` are left unchanged.
- Unbind restores only the roles changed by this adapter, in reverse order.
- Binding does not edit the six other tracks' step data. The official role API snapshots/restores each affected project row and track state.

**Important upstream side effect:** `groove_track_role_set()` pauses the session, snapshots track/project state, may allocate or recreate an engine, resets the track, refreshes audio/mix state, and calls `groove_edit_forget_history()`. Binding/unbinding can therefore invalidate project undo history. If a role change fails, the adapter rolls back prior changes. If rollback/unbind is incomplete, it retains the outstanding role mask and blocks lane operations/transport until unbind succeeds on retry.

Role preferences are session/device state, not a project-format rewrite. Do not treat the two-lane binding as a persistent project setting.

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

## Transport contract

- Start posts MIDI Start (`0xFA`) through the official `fm1_midi_post()` local-input queue, matching the official screen's Start path.
- Stop uses `groove_screen_mmc(1)`, preserving the official screen's Stop and recording cleanup behavior.
- `groove_screen_mmc(2)` is Continue (`0xFB`), not Start; the adapter no longer uses it for a fresh Play action.
- The current UI exposes Start/Stop, not a separate pause/Continue state.

## Adapter API

`firmware/fm1lab_sequencer.c` provides bind/unbind, state read, tempo/swing/length editing, step editing, lane selection, patch assignment from the four-patch Instrument, patch level and mute application, and transport start/stop. Patch assignment renders the selected Instrument patch through the macro renderer, imports the resulting DX7 voice into the bound track, then applies its level and enabled state. All functions are control-task operations. The adapter does not perform flash I/O; the existing project system remains responsible for persistence and undo.

## Native screen integration

The build-time screen patch adds a dedicated two-lane page entered with the physical SEQ button from the Instrument home screen. SELECT moves the 64-step cursor; PRESETS selects lane A/B; ALGORITHM selects one of the four Instrument patches for that lane; KNOB 1 enables/disables the selected step, KNOB 2 changes pitch, KNOB 3 changes velocity, and KNOB 4 changes gate. PLAY toggles the native sequencer transport. The display shows a 16-step window of the selected lane position, with a fixed 64-step timeline and the native loop length.

The sequencer page binds the first two normal DX7 tracks in the current project. Binding uses the official track-role API to take other NORMAL tracks temporarily OFF; this can invalidate project undo history as described above. Explicit step edits change the selected steps on the two bound tracks. Selecting an Instrument patch for a lane imports that patch into the corresponding project track, as expected for a sequencer track assignment. Leaving the sequencer stops playback and restores the roles changed by the adapter.

## Remaining integration

The native screen patch and sequencer adapter are included in both the simulator build and the Windows device-build staging script. CI compiles the official M-VAVE browser engine and exercises the host-side C adapter tests. This is not yet a verified JieLi device firmware build or a flashable hardware image. The device build still requires the matching JieLi toolchain, and actual FM1 hardware validation remains outstanding. Browser editing remains a development surface, not the physical firmware deliverable.
