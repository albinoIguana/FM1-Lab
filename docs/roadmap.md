# FM1-Lab implementation roadmap

## In place
- [x] Use M-VAVE's official FM1 open-source repository and SDK as the source foundation.
- [x] Pin the official source as a Git submodule.
- [x] Define one Instrument as exactly four independent six-operator DX7 patches.
- [x] Store each patch in M-VAVE's native 156-byte expanded voice format.
- [x] Build non-destructive Brightness, Body, Motion and Edge macro transforms.
- [x] Derive carrier roles from M-VAVE's actual algorithm routing table.
- [x] Add operator editing for oscillator mode, coarse/fine, output, and four-stage envelopes.
- [x] Connect the browser workshop to M-VAVE's own simulator and audio worklet.

## Next: finish the Instrument workflow
- [ ] Verify the browser build and sound path end-to-end on desktop and iOS Safari.
- [ ] Add patch enable/mute, patch level and per-patch performance state.
- [ ] Complete Instrument library save/load, rename, duplicate and export/import behavior.
- [ ] Add macro range/scaling controls and saveable per-Instrument macro assignments.

## Device firmware integration
- [ ] Move the four-patch Instrument data model into M-VAVE's on-device application layer.
- [ ] Map the physical preset encoder to Instrument selection.
- [ ] Map the four physical patch controls to patch selection.
- [ ] Add the four macro controls to the device UI with algorithm-aware carrier/modulator behavior.
- [ ] Persist Instruments in FM1 storage without breaking existing projects, samples or DX7 voice banks.
- [ ] Add device-side tests for patch isolation, macro limits, save/load integrity and MIDI behavior.
- [ ] Build with JieLi toolchain 2.5.3 and verify on actual FM1 hardware before calling it flashable.

The browser simulator is a test and audition harness built from M-VAVE's official source. It is not a substitute for implementing the custom Instrument workflow in the device application.
