# FM1-Lab memory budget audit

**Target:** M-VAVE FM1, 1 MiB SPI NOR and 512 KiB internal SRAM  
**Build:** native fm1-base + Felucca-BSP + one pinned Felucca FM6 core  
**Branch baseline:** `ffdf13cbd251f46f934a4b28c1de1d36d7dfbfbd`  
**Audit status:** linker regions and current section totals reconciled against the pinned linker and builder; exact section-byte reporting added to the next device build.

## 1. Flash is not one 1 MiB application slot

The active Felucca-family linker/loader reserves a **581,564-byte XIP application slot**. The rest of the 1 MiB flash is partitioned for boot/update, VM/configuration, Bluetooth data, user data and device reservations. The app must not treat those other regions as extra firmware capacity.

The last successful native build reported an application image of **153,380 bytes**, leaving **428,184 bytes** in the XIP slot. The packaged `.fwsc` file is not the installed application image size and is not used for this calculation.

The app's persistent Instrument store is separately reserved in the 12-sector `0xF0000..0xFBFFF` pool (48 KiB). Felucca projects, samples, user preset banks, autosave, voice banks, global settings and OTA staging remain outside that allocation. See [REBUILD_AUDIT.md](REBUILD_AUDIT.md); do not move these ranges without re-auditing the pinned BSP.

## 2. SRAM is split into independently enforced regions

The pinned `base/device/app.ld` defines these regions:

| Region | Capacity | Purpose |
|---|---:|---|
| `.ram_text` | 32,768 B | Code that must execute from RAM, including the radio crystal ramp and selected audio/runtime code |
| `.data + .bss` | 98,304 B | Initialized and zero-initialized globals, including app state, FM6 voices and platform state |
| `.pool` | 344,064 B | Large zero-initialized buffers, such as the graphics canvas and future shared FX delay/reverb buffers |
| Guarded stacks | 32,768 B | User stack 24,320 B, system stack 7,936 B, plus two 256-byte guard bands |
| `.noinit` | 15,696 B | Reset-persistent runtime/diagnostic state |
| Boot-reserved tail | 688 B | Boot information, update mailbox and vectors |

These regions total 512 KiB. They are not interchangeable. In particular, spare `.pool` bytes do not prevent a `.bss` overflow, and the stack/boot reservations are not app allocation space.

The linker file is the authority for addresses. The builder separately checks the 96 KiB `.data + .bss` limit and the 336 KiB pool limit, preserving at least 8,192 bytes of pool headroom.

## 3. Last successful build: what the numbers actually mean

| Measurement | Used | Capacity | Remaining |
|---|---:|---:|---:|
| XIP application image | 153,380 B | 581,564 B | 428,184 B |
| `.data + .bss` | 43,752 B | 98,304 B | 54,552 B |
| `.pool` | 69,384 B | 344,064 B | 274,680 B |
| `.pool` after the builder's mandatory 8 KiB reserve | — | — | 266,488 B available above the reserve |

**Important correction:** the 69,384-byte `.pool` figure is not an FM6 voice/audio heap. The pinned graphics implementation declares a 16-bit canvas of `240 × 144` pixels in `.pool`, which is **69,120 bytes**. That accounts for all but 264 bytes of the reported pool use. The pool has roughly 268 KiB of remaining allocatable space after preserving the builder's 8 KiB safety reserve.

The FM6 note state is instead in the regular `.bss` region: the core's `fm6_note_t` is 244 bytes, so 32 live note records account for **7,808 bytes**, before the FM1-Lab voice wrapper, four patch records, LFO state and other engine/platform globals. The 43,752-byte total includes all linked app and platform globals; it is not an FM6-only figure.

### Exact ELF section sizes from the new build report

The 2026-11-05 device build emitted the following section sizes directly from `app/build/fm1lab.elf`:

| ELF section | Used | Region capacity |
|---|---:|---:|
| `.text` (XIP code + const data) | 146,888 B | Included in the 581,564 B XIP image slot |
| `.ram_text` | 3,556 B | 32,768 B |
| `.data` | 2,936 B | Part of 98,304 B |
| `.bss` | 40,808 B | Part of 98,304 B |
| `.pool` | 69,384 B | 344,064 B |
| `.noinit` | 1,836 B | 15,696 B |

The builder's address-based `.data + .bss` check reports 43,752 B; the raw ELF section sizes sum to 43,744 B. The 8-byte difference is retained in the conservative budget rather than silently discarded. The `.ram_text` region has 29,212 B unused, and `.noinit` has 13,860 B unused. The application image size is exactly 153,380 B (146,888 + 3,556 + 2,936), before package wrapping.

The FM6 core's two 32-sample operator buses are shared static buffers (256 bytes total), not duplicated per voice. Generated lookup tables are `const` data and are placed with XIP `.text/.rodata`, not copied into the 96 KiB RAM region. Only one FM6 core is linked; the four patches are data/voice pools, not four copies of the engine.

## 4. What this means for the next features

- **Four-patch FM6:** current state fits the existing 96 KiB region with 54,552 bytes of link-time headroom. Do not reduce the 8-voice-per-patch design based on the package ZIP size.
- **FX:** use one shared effects rack with per-patch sends, not four duplicate effect engines. Its large delay/reverb buffers can be allocated from `.pool`; preserve the 8 KiB mandatory reserve. DSP code consumes XIP flash and render time, while mutable FX state and control structures consume RAM.
- **Two-lane sequencer:** keep step/event state compact and statically bounded. It should not require a large audio buffer or a second copy of the synth.
- **UI/editor:** the current full-width canvas is the dominant `.pool` allocation. It is not necessary to remove it for memory reasons today, but a strip/tile renderer is a clear fallback if future audio buffers need the space.
- **CPU:** memory headroom does not prove real-time headroom. The active build does not enable `cpu2 = true`; the second core is not currently distributing FM6 work. Device CPU percentage and xrun telemetry remain the required proof under dense four-layer playing and effects.
- **Stack:** linker capacity is reserved, but actual stack high-water marks still need a device run under MIDI, BLE and dense synthesis. Do not count stack space as free pool.

## 5. Measurement improvement added

The native build workflow now prints ELF section byte sizes for `.text`, `.ram_text`, `.data`, `.bss`, `.pool` and `.noinit`, plus the app image size and region limits. This removes the previous ambiguity where the log's combined `.data + .bss` and `.pool` totals were the only app memory measurements.

This report deliberately distinguishes:
- source/linker-verified region capacities;
- the last successful build's reported totals;
- derived allocations (canvas and FM6 note records);
- values that still require physical-device measurement (stack high-water, runtime heap use, CPU load and xruns).

No hardware flashing or on-device memory/CPU validation is claimed.


## 5. Current-source re-audit warning (2026-11-05)

The section totals above are a historical successful-build snapshot, not a verified measurement of the current feature-branch head. A source audit of the current `app/dsp/fm6.c` finds the following statically declared `.pool` arrays:

| Buffer | Bytes |
|---|---:|
| Delay: 8,192 × int32 | 32,768 |
| Chorus: 2,048 × int32 | 8,192 |
| Reverb comb A: 2,048 × int32 | 8,192 |
| Reverb comb B: 4,096 × int32 | 16,384 |
| Reverb comb C: 8,192 × int32 | 32,768 |
| **Current FM6 FX buffers** | **98,304** |

The previously documented graphics canvas is 240 × 144 × 2 = 69,120 bytes. Those known allocations alone imply at least **167,424 bytes** of `.pool` use before any other pool allocations. This does not establish the final linked total; only the ELF from a build of the exact current branch can do that.

Accordingly, do not use the 69,384-byte historical `.pool` figure or its derived remaining-headroom number to approve new buffers. The next native device build must refresh every ELF section total and confirm the builder's 8 KiB pool reserve. The older figures remain here as historical baseline data, not current capacity claims.
