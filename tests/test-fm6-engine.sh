#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
FELUCCA="$ROOT/upstream/felucca"
TMP="$(mktemp -d)"
BIN="$TMP/fm6-engine-test"
trap 'rm -rf "$TMP"' EXIT

if [[ ! -f "$FELUCCA/firmware/engines/tools/gen_engine_tables.py" ]]; then
  echo "Pinned Felucca-Engines submodule is missing; initialize recursive submodules." >&2
  exit 1
fi

python3 "$FELUCCA/tools/gen_tables.py" "$TMP/felucca_tables.h"
python3 "$FELUCCA/tools/gen_fm6_patches.py" "$TMP/felucca_fm6.h"

cc -std=c11 -O2 -DFM6_TEST -Wall -Wextra -Werror -Wno-unused-const-variable -pthread \
  -I"$ROOT/app/dsp" -I"$TMP" \
  "$ROOT/app/dsp/fm6.c" "$ROOT/tests/fm6_engine.test.c" -o "$BIN"

DX7_BIN="$TMP/fm6-dx7-audit"
cc -std=c11 -O2 -DFM6_TEST -Wall -Wextra -Werror -Wno-unused-const-variable \
  -I"$ROOT/app" -I"$ROOT/app/dsp" -I"$TMP" \
  "$ROOT/app/dsp/fm6.c" "$ROOT/tests/fm6_dx7_library_audit.test.c" -o "$DX7_BIN"
"$DX7_BIN"
"$BIN"

# Mirror Clipper's CORES=2/ThreadSanitizer regression: the CPU2 host twin
# really runs concurrently with CPU0, and any unsynchronized shared access
# fails CI rather than being hidden by the serial simulator.
if [[ "${FM6_TSAN:-0}" == "1" ]]; then
  TSAN_BIN="$TMP/fm6-engine-tsan"
  cc -std=c11 -O1 -g -DFM6_TEST -Wall -Wextra -Werror -Wno-unused-const-variable \
    -fsanitize=thread -fno-omit-frame-pointer -pthread \
    -I"$ROOT/app/dsp" -I"$TMP" \
    "$ROOT/app/dsp/fm6.c" "$ROOT/tests/fm6_engine.test.c" -o "$TSAN_BIN"
  TSAN_OPTIONS=halt_on_error=1 "$TSAN_BIN"
fi
