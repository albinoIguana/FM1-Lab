#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="$(mktemp)"
trap 'rm -f "$BIN"' EXIT
cc -std=c11 -Wall -Wextra -Werror -I"$ROOT/firmware" \
  "$ROOT/firmware/fm1lab_instrument.c" \
  "$ROOT/firmware/fm1lab_bank_metadata.c" \
  "$ROOT/tests/fm1lab_instrument.test.c" \
  "$ROOT/tests/fm1lab_bank_metadata.test.c" -o "$BIN"
"$BIN"
