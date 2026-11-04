#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
INSTRUMENT_BIN="$(mktemp)"
METADATA_BIN="$(mktemp)"
CONTROLS_BIN="$(mktemp)"
trap 'rm -f "$INSTRUMENT_BIN" "$METADATA_BIN" "$CONTROLS_BIN"' EXIT
cc -std=c11 -Wall -Wextra -Werror -I"$ROOT/firmware" \
  "$ROOT/firmware/fm1lab_instrument.c" \
  "$ROOT/tests/fm1lab_instrument.test.c" -o "$INSTRUMENT_BIN"
"$INSTRUMENT_BIN"
cc -std=c11 -Wall -Wextra -Werror -I"$ROOT/firmware" \
  "$ROOT/firmware/fm1lab_bank_metadata.c" \
  "$ROOT/tests/fm1lab_bank_metadata.test.c" -o "$METADATA_BIN"
"$METADATA_BIN"

cc -std=c11 -Wall -Wextra -Werror -I"$ROOT/firmware" "$ROOT/tests/fm1lab_physical_controls.test.c" -o "$CONTROLS_BIN"
"$CONTROLS_BIN"
echo "FM1-Lab physical control mapping tests passed."
