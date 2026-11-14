#!/usr/bin/env python3
"""Apply the FM1-Lab bounded CPU2 worker timeout to the pinned fm1-base builder.

This changes the generated build configuration in the checked-out submodule,
not the vendor source or the BSP header. Re-running it is safe.
"""
from pathlib import Path

build = Path(__file__).resolve().parents[1] / "base" / "tools" / "build.py"
source = build.read_text()
old = "flags += cpu2_stack_flags()"
new = old + '\n        flags += ["-DFM1_CPU1_TIMEOUT_US=1500u"]'
if old in source:
    source = source.replace(old, new, 1)
elif new not in source:
    raise SystemExit("Expected CPU2 stack flag site not found in pinned fm1-base tools/build.py")
build.write_text(source)
print("FM1_CPU1_TIMEOUT_US=1500 us (initial hardware qualification ceiling)")
