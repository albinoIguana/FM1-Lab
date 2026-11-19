# FM1-Lab

**FM1-Lab is an independent mixed-synthesis Instrument for the M-VAVE FM1.** The firmware foundation is the open M-VAVE FM1 source/SDK and the pinned Felucca FM6 implementation. The Instrument model, engine-native patch data, editor and workflow are FM1-Lab work.

## Instrument model

One Instrument contains exactly four fixed-role patch slots:

1. **FM6** — six-operator FM voice, using the expanded M-VAVE/DX7-compatible voice format.
2. **Wavetable** — native wavetable-engine patch.
3. **PSG** — native programmable-sound-generator patch.
4. **Subtractive** — native subtractive-synth patch.

The four slots are simultaneous layers of one Instrument, not four interchangeable DX7 voices. The library contains 4 banks × 8 Instruments (32 Instruments). Each slot retains its own patch name/identity, enable state, level, pan, tuning and macros. The current native Wavetable, PSG and Subtractive factory catalogs each contain four starter patches; their source records live under `patches/library/<engine>/`. DX7 archive material is kept separately under `patches/library/dx7/`.

The FM6 slot uses the curated 128-voice DX7-compatible library. The other three slots store and apply engine-native parameter records; legacy DX7-only Instrument records migrate by retaining the old FM voice in slot 1 and assigning valid defaults to the three newly typed slots. Old DX7 bytes are never reinterpreted as Wavetable, PSG or Subtractive parameters.

## Performance and audio

- One live sequencer track. Older two-lane save records are decoded at the persistence boundary: lane 0 is retained and lane 1 is intentionally retired.
- Arpeggiator and Maelstrom/Chaos remain active features.
- Four retained stereo effects: Chorus, Delay, Reverb and Phaser.
- The previously explored second sequencer lane, Amp/Drive, parametric EQ and master compressor are retired from the active product scope.
- The DSP uses fixed-size voice/state storage and allocation-free fixed-point synthesis on the device audio path.

## Editing and persistence

The physical patch slots have fixed engine identities. Selecting a preset in a slot replaces that slot's sound recipe while preserving its mixer/performance state. Instrument and performance data use versioned, CRC-protected persistence records. Compatibility structures for older two-lane settings remain persistence-only and are converted into the one-track live state before reaching runtime code.

The browser workshop and native device application are separate surfaces. The active native firmware is built against the pinned fm1-base/Felucca target; the browser simulator is useful for development but is not proof of device audio behavior.

## Validation status

GitHub Actions runs the firmware-specific host regression suite, app-input route tests, mixed-engine audio tests, retained-FX combination tests, ARP/Maelstrom checks, persistence/migration tests and the native FM1 firmware build. A successful CI build proves compilation and host-side regression coverage; it does **not** prove physical FM1 playback, hardware timing, flash migration/recovery or real-time CPU/xrun behavior.

Before installing any firmware, make a complete 1 MiB flash backup using the documented recovery procedure. The web installer itself does not create that backup. Physical-device audition and performance validation remain outstanding.

## Build

The native firmware build uses the pinned M-VAVE/Felucca sources and matching JieLi toolchain. GitHub Actions publishes a `packages` artifact containing the firmware package, installer and build diagnostics. Verify the build identity and package checksum before use.

## Licensing

Preserve the upstream M-VAVE, fm1-base and Felucca licenses, third-party notices and SDK redistribution terms when building or distributing firmware. See the source and license notices in the pinned dependencies.
