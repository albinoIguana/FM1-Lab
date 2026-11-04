#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SOURCE="$ROOT/vendor/m-vave-fm1"
BIN="$(mktemp)"
trap 'rm -f "$BIN"' EXIT

cc -std=c11 -O2 -Wall -Wextra -Werror -Wno-array-parameter \
  -DFM1_HOST_TEST -DFM1_DSP_ENTRY= \
  -I"$ROOT/firmware" \
  -I"$SOURCE/app/synth" \
  -I"$SOURCE/sdk/include" \
  -I"$SOURCE/web/synth" \
  -I"$SOURCE/third_party/synth" \
  "$ROOT/firmware/fm1lab_audio_engine.c" \
  "$ROOT/tests/fm1lab_audio_engine.test.c" \
  "$SOURCE/web/synth/dx7synth.c" \
  "$SOURCE/web/synth/dx7core.c" \
  "$SOURCE/web/synth/dx7note.c" \
  "$SOURCE/web/synth/dx7env.c" \
  "$SOURCE/web/synth/dx7lfo.c" \
  "$SOURCE/web/synth/dx7pitchenv.c" \
  "$SOURCE/web/synth/dx7math.c" \
  "$SOURCE/web/synth/fixed.c" \
  "$SOURCE/web/synth/adsr.c" \
  -o "$BIN"

"$BIN"
