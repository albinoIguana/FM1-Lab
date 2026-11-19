#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
cc -std=c11 -O2 -Wall -Wextra -Werror \
  -I"$ROOT/app/dsp" \
  "$ROOT/app/dsp/fm1lab_synth_engines.c" "$ROOT/app/dsp/fm1lab_fm4.c" \
  "$ROOT/tests/bench_mixed_synth.c" \
  -o "$TMP/bench-mixed-synth"
"$TMP/bench-mixed-synth"
