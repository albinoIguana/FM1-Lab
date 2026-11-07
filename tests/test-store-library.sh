#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
STORE_BIN="$(mktemp)"
trap 'rm -f "$STORE_BIN"' EXIT
cc -std=c11 -Wall -Wextra -Werror -I"$ROOT/tests/fakes" -I"$ROOT/app" \
  "$ROOT/firmware/fm1lab_instrument.c" "$ROOT/app/fm1lab_store_library.c" \
  "$ROOT/tests/fm1lab_store_library.test.c" -o "$STORE_BIN"
"$STORE_BIN"
echo "FM1-Lab 32-Instrument pooled storage tests passed."
