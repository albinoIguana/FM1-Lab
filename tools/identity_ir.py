"""Pass LLVM IR through unchanged so fm1-base can retain optimized IR for stack analysis."""
from pathlib import Path
import sys

Path(sys.argv[2]).write_bytes(Path(sys.argv[1]).read_bytes())
