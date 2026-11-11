#!/usr/bin/env python3
"""Apply the narrowly scoped dual-FM1-flash backport to the pinned BSP.

This intentionally does not advance the fm1-base or felucca-bsp gitlinks. It
patches only the old BSP's app-side NOR completion path and its recovery tool.
The input anchors are checked so an upstream change cannot be silently mangled.
"""
from pathlib import Path
import argparse
import sys

ROOT = Path(__file__).resolve().parents[1]
BSP = ROOT / "base" / "bsp"
DRIVER = BSP / "chip" / "fm1_flash.h"
RESCUE = BSP / "tools" / "fm1_rescue.py"
WAIT = ROOT / "patches" / "fm1_flash_wait.h"

RESET_HELPER = r'''
/* A part still BUSY at the extended bound: reset the NOR, wait for tRST,
 * then reset the processor. All instructions execute from RAM with XIP off. */
RAMINL void fl_reset_busy(void)
{
    uint32_t t0;
    volatile uint8_t cmd = 0x66;
    for (;;) {
        fl_cs(0); fl_tx(cmd); fl_cs(1);
        if (cmd == 0x99) break;
        cmd = 0x99;
    }
    t0 = T4_CNT;
    while (T4_CNT - t0 < 24u * 20000u) ;
    REG32(0x10000u) |= 1u << 4;
    for (;;) ;
}
#include "fm1_flash_wait.h"

'''

def patch_driver(apply: bool) -> bool:
    if not DRIVER.exists():
        raise SystemExit(f"missing nested BSP driver: {DRIVER}")
    source = DRIVER.read_text()
    already = ('#include "fm1_flash_wait.h"' in source and
               'fl_wait_issued(issued, 500000u, 2000000u, took_us)' in source and
               'fl_wait_issued(issued, 10000u, 50000u, took_us)' in source)
    if already:
        return True
    if not apply:
        return False

    marker = "/* ---- entry points: IRQs OFF, args/buffers in RAM ---- */"
    if source.count(marker) != 1:
        raise SystemExit("BSP driver drift: expected exactly one FM1 entry-point marker")
    if "fl_wait_ready(500000u, took_us)" not in source or "fl_wait_ready(10000u, took_us)" not in source:
        raise SystemExit("BSP driver drift: expected legacy erase/program completion waits")
    source = source.replace(marker, RESET_HELPER + marker, 1)

    erase_old = """        fl_cs(0); fl_tx(0x20); fl_addr(off); fl_cs(1);
        fl_inval(off, 4096u);
        rc = fl_wait_ready(500000u, took_us);     /* generic NOR max 400 ms */"""
    erase_new = """        uint32_t issued = T4_CNT;
        fl_cs(0); fl_tx(0x20); fl_addr(off); fl_cs(1);
        fl_inval(off, 4096u);
        rc = fl_wait_issued(issued, 500000u, 2000000u, took_us);"""
    prog_old = """        fl_cs(0); fl_tx(0x02); fl_addr(off);
        for (i = 0; i < n; i++) fl_tx(src[i]);
        fl_cs(1);
        fl_inval(off, n);
        rc = fl_wait_ready(10000u, took_us);      /* generic NOR max 3 ms */"""
    prog_new = """        uint32_t issued = T4_CNT;
        fl_cs(0); fl_tx(0x02); fl_addr(off);
        for (i = 0; i < n; i++) fl_tx(src[i]);
        fl_cs(1);
        fl_inval(off, n);
        rc = fl_wait_issued(issued, 10000u, 50000u, took_us);"""
    if source.count(erase_old) != 1 or source.count(prog_old) != 1:
        raise SystemExit("BSP driver drift: erase/program blocks do not match the audited pinned source")
    source = source.replace(erase_old, erase_new, 1).replace(prog_old, prog_new, 1)
    DRIVER.write_text(source)
    return True

def patch_rescue(apply: bool) -> bool:
    if not RESCUE.exists():
        raise SystemExit(f"missing nested BSP recovery tool: {RESCUE}")
    source = RESCUE.read_text()
    if "FLASH_IDS = (0x856014, 0xB36014)" in source and "fid not in FLASH_IDS" in source:
        return True
    if not apply:
        return False
    size_line = "FLASH_SIZE = 0x100000"
    old_guard = """    if (key, typ, fid) != (0x980F, 3, 0x856014):
        raise Fail("this is not the FM-1's chip and flash (expected key 980F, type 3, id 856014); nothing written")"""
    new_guard = """    if key != 0x980F or typ != 3 or fid not in FLASH_IDS:
        allowed = ", ".join(f"{value:06X}" for value in FLASH_IDS)
        raise Fail(f"this is not a supported FM-1 flash (expected key 980F, type 3, id in {allowed}); nothing written")"""
    if source.count(size_line) != 1 or source.count(old_guard) != 1:
        raise SystemExit("BSP rescue drift: expected audited single-ID guard was not found")
    source = source.replace(size_line, size_line + "\nFLASH_IDS = (0x856014, 0xB36014)  # Puya P25Q80H and UCUN UC25HQ80, both 1 MiB", 1)
    source = source.replace(old_guard, new_guard, 1)
    RESCUE.write_text(source)
    return True

def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true", help="fail unless both backports are already applied")
    args = parser.parse_args()
    if not WAIT.exists():
        raise SystemExit(f"missing shared wait helper: {WAIT}")
    if args.check:
        ok = patch_driver(False) and patch_rescue(False)
        if not ok:
            print("dual-flash BSP backport is missing", file=sys.stderr)
            return 1
        print("dual-flash BSP backport: present")
        return 0
    patch_driver(True)
    patch_rescue(True)
    # Copy the tested helper beside the BSP header that includes it.
    target = BSP / "chip" / "fm1_flash_wait.h"
    target.write_bytes(WAIT.read_bytes())
    print("applied targeted dual-flash BSP backport (no submodule revisions changed)")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
