#!/usr/bin/env python3
"""Identity IR pass for fm1-base static stack analysis."""
import pathlib
import shutil
import sys
if len(sys.argv) not in (3, 4):
    raise SystemExit("usage: identity_ir.py INPUT.ll OUTPUT.ll [UNIT]")
shutil.copyfile(pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2]))
