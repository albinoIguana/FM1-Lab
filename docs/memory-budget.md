# FM1-Lab memory budget audit

**Target:** M-VAVE FM1, 1 MiB SPI NOR and 512 KiB internal SRAM  
**Build:** native fm1-base + Felucca-BSP + one pinned Felucca FM6 core  
**Code baseline:** `80f0842659a90dd56b67e92f76cdf8d1598a615f` (2026-11-06)  
**Audit status:** native target ELF section totals verified by CI. Physical-device CPU, xrun, stack high-water and listening qualification remain outstanding.

## 1. Flash is not one 1 MiB application slot

The active Felucca-family linker/loader reserves a **581,564-byte XIP application slot**. The rest of flash is partitioned for boot/update, VM/configuration, Bluetooth data, user data and device reservations. Those regions are not spare firmware capacity.

The latest successful native device build emitted an application image of **189,604 bytes**, leaving **391,960 bytes** in the XIP slot. The packaged `.fwsc` file is not the installed application image size and is not used for this calculation.

The Instrument store is separately reserved in the 12-sector `0xF0000..0xFBFFF` pool (48 KiB). Do not move these ranges without re-auditing the pinned BSP; see [REBUILD_AUDIT.md](REBUILD_AUDIT.md).

## 2. SRAM is split into independently enforced regions

The pinned `base/device/app.ld` defines:

| Region | Capacity | Purpose |
|---|---:|---|
| `.ram_text` | 32,768 B | Code that must execute from RAM |
| `.data + .bss` | 98,304 B | Initialized and zero-initialized globals, FM6 voices and platform state |
| `.pool` | 344,064 B | Large static buffers, graphics canvas and shared stereo FX storage |
| Guarded stacks | 32,768 B | User stack 24,320 B, system stack 7,936 B, plus two 256-byte guard bands |
| `.noinit` | 15,696 B | Reset-persistent runtime/diagnostic state |
| Boot-reserved tail | 688 B | Boot information, update mailbox and vectors |

These regions total 512 KiB and are not interchangeable. The builder separately checks the 96 KiB `.data + .bss` limit and 336 KiB pool limit, preserving at least 8,192 bytes of pool headroom.

## 3. Latest successful native target build

CI run: [Native FM1 build](https://github.com/albinoIguana/FM1-Lab/actions/runs/42282415707)

| Measurement | Used | Capacity | Remaining |
|---|---:|---:|---:|
| XIP application image | 189,604 B | 581,564 B | 391,960 B |
| `.data + .bss` (builder region check) | 47,336 B | 98,304 B | 50,968 B |
| `.pool` | 275,264 B | 344,064 B | 68,800 B |
| `.pool` after mandatory 8 KiB reserve | — | — | 60,608 B |

### Exact ELF section sizes

| ELF section | Used | Region capacity |
|---|---:|---:|
| `.text` | 183,080 B | Included in XIP image slot |
| `.ram_text` | 3,556 B | 32,768 B |
| `.data` | 2,968 B | Part of 98,304 B |
| `.bss` | 44,360 B | Part of 98,304 B |
| `.pool` | 275,264 B | 344,064 B |
| `.noinit` | 1,836 B | 15,696 B |
| `app.bin` | 189,604 B | 581,564 B |

The raw `.data + .bss` section sum is 47,328 B (2,968 + 44,360); the builder's address-based region check reports 47,336 B, including 8 bytes of alignment. The `.ram_text` region has 29,212 B unused, and `.noinit` has 13,860 B unused. The image size is exactly 189,604 B (183,080 + 3,556 + 2,968), before package wrapping.

The FM6 core's two 32-sample operator buses are shared static buffers (256 bytes total), not duplicated per voice. The core's `fm6_note_t` is 244 bytes; 32 live note records account for 7,808 bytes within `.bss`, before the FM1-Lab voice wrappers, four patch records, LFO state and platform globals. Only one FM6 core is linked.

## 4. Pool composition and Instrument-wide FX

The pinned graphics implementation declares a 16-bit canvas of `240 × 144` pixels (**69,120 bytes**). The four stereo FX processors consume **196,608 bytes** of feedback storage. Those known allocations total **265,728 bytes**, leaving 9,536 bytes for other pool users.

Amp/Drive and the three-band parametric EQ now run once on the complete stereo Instrument bus after the shared FX returns; they do not allocate four copies of patch-insert state or filter histories. Their 14-byte control state is stored once per Instrument (32 Instruments) in the versioned `OBJ_INSERTS` sidecar. The compact V3 on-flash journal is 936 bytes. Static migration buffers remain large enough to read the older V1/V2 sidecars (3,624-byte record buffer plus 1,806-byte snapshot scratch); those buffers are included in the pool total.

The 479-byte Instrument voice-bank record is unchanged. The sidecar's active/fallback snapshots remain protected by CRC plus bank CRC/generation. Legacy V1/V2 sidecars containing four patch insert records per Instrument migrate to one global state: identical old patch settings are retained; divergent values are safely bypassed and averaged for later manual recovery.

## 5. What this means

- **Four-patch FM6:** the current state fits the 96 KiB region with 50,968 bytes of link-time headroom. Keep the documented eight-voice-per-patch design.
- **FX:** continue using one shared stereo effects rack with per-patch sends, not four duplicate effect engines. Preserve the mandatory 8 KiB pool reserve.
- **Sequencer:** keep the two lanes' step/event state compact and statically bounded.
- **UI:** the full-width canvas is the dominant pool allocation. A strip/tile renderer remains a fallback if future audio buffers need space; it is not required by this build's memory budget.
- **CPU:** memory headroom does not prove real-time headroom. The second core is not currently distributing FM6 work. Device CPU percentage and xrun telemetry must be measured under dense four-layer playing and effects.
- **Stack:** actual stack high-water marks still need a device run under MIDI, BLE and dense synthesis. Do not count stack capacity as free pool.

## 6. Measurement boundaries

The native build workflow prints ELF section sizes for `.text`, `.ram_text`, `.data`, `.bss`, `.pool` and `.noinit`, plus the application image size and region limits. This distinguishes linker-verified capacity from derived allocations and values requiring physical-device measurement.

This audit verifies link-time memory only. It does **not** claim physical-device flashing, worst-case callback timing, stack high-water, runtime heap use, CPU load, xrun-free operation or listening qualification.
