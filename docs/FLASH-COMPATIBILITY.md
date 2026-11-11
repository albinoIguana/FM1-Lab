# FM1 dual-flash compatibility and validation

## Supported hardware population

| FM1 flash | JEDEC ID | Density | Geometry |
|---|---:|---:|---|
| Puya P25Q80H | `85 60 14` (`0x856014`) | 8 Mbit / 1 MiB | 256-byte page, 4 KiB sector |
| UCUN UC25HQ80 | `B3 60 14` (`0xB36014`) | 8 Mbit / 1 MiB | 256-byte page, 4 KiB sector |

The audited FM1 platform source identifies the same AC791N host family for both populations. The compatibility difference handled here is the SPI NOR vendor/JEDEC identity, not a separate application CPU target.

Both parts use the standard SPI NOR commands already used by the FM1 driver: JEDEC ID `9F`, status `05`, write-enable `06`, page-program `02`, 4 KiB erase `20`, and fast-read `0B`. Unknown IDs remain fail-closed: no flash-backed persistence or update write is permitted.

## Firmware changes in this candidate

- Retain the fm1-base allowlist for `0x856014` and `0xB36014`.
- Backport the upstream busy-safe completion logic without advancing the BSP submodule to a revision containing unrelated changes.
- Start erase/program timing at command issue; return the legacy late-ready code only after BUSY clears.
- Extend the defensive completion windows to 2 s for sector erase and 50 ms for page program.
- If BUSY persists to the extended bound, reset the NOR with `66h/99h`, wait 20 ms for reset recovery, then reset the processor. Never restore XIP while the NOR still reports BUSY.
- Update the recovery utility's flash-ID allowlist to both supported devices.

The extended waits are defensive recovery limits based on field reports and upstream implementation. They are not a claim that every UCUN part is slower than the Puya part.

## Automated test layers

Run locally or in CI:

```sh
python3 tools/apply-fm1-flash-compat.py
bash tests/test-flash-compat.sh
```

1. **JEDEC policy test:** compile the exact fm1-base allowlist header; accept both supported IDs and reject unknown IDs and a mismatched density byte.
2. **Completion-wait host test:** exercise normal completion, completion after the normal bound, completion just inside the extended bound, permanently busy parts, reset at the extended bound, and TIMER4 wraparound for erase and program limits.
3. **NOR protocol model:** apply identical read/program/erase sequences to both IDs; verify write-enable consumption, 1-to-0 programming, page boundaries, sector alignment, and 1 MiB bounds. This is a protocol contract model, not a substitute for testing the actual silicon.
4. **Recovery-tool policy test:** parse the real recovery utility and verify both IDs are accepted.
5. **BSP patch integrity:** the backport script checks known source anchors and fails closed if the pinned upstream driver/recovery tool has drifted.
6. **Firmware build:** the native build applies the backport before compiling the actual FM1 image, then runs the existing engine, stack, and memory checks.

## Physical qualification still required

Automated tests cannot prove the electrical behavior of either flash chip. Before a release candidate is offered for flashing, qualify one physical FM1 from each population:

- Read and record JEDEC ID and confirm density.
- Back up the entire 1 MiB flash before any write test.
- Cold boot; confirm settings and instrument data load.
- Save settings and instrument data; power-cycle; confirm byte-for-byte persistence.
- Exercise repeated page writes, sector erases, and readback verification in the allocated data region only.
- Install a firmware update; verify reboot and retained settings.
- Exercise the documented ROM/recovery path with a known-good stock package; confirm the protected boot head and device-data regions remain untouched.
- Repeat erase/program tests under normal and warm operating conditions; capture operation durations and error codes.
- Confirm unknown JEDEC IDs remain read-only and do not attempt erase/program.

No physical qualification is implied by a green CI run. This branch remains a diagnostic candidate; it does not change the production installer or authorize a flash.
