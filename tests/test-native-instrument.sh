#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="$(mktemp)"
trap 'rm -f "$BIN"' EXIT
cc -std=c11 -Wall -Wextra -Werror -I"$ROOT/firmware" \
  "$ROOT/firmware/fm1lab_instrument.c" \
  "$ROOT/tests/fm1lab_instrument.test.c" -o "$BIN"
"$BIN"
