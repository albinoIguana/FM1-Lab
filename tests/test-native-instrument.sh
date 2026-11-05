#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
INSTRUMENT_BIN="$(mktemp)"
METADATA_BIN="$(mktemp)"
CONTROLS_BIN="$(mktemp)"
LIBRARY_BIN="$(mktemp)"
RUNTIME_BIN="$(mktemp)"
SEQUENCER_BIN="$(mktemp)"
SETTINGS_BIN="$(mktemp)"
trap 'rm -f "$INSTRUMENT_BIN" "$METADATA_BIN" "$CONTROLS_BIN" "$LIBRARY_BIN" "$RUNTIME_BIN" "$SEQUENCER_BIN" "$SETTINGS_BIN"' EXIT
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

cc -std=c11 -Wall -Wextra -Werror \
  -I"$ROOT/firmware" -I"$ROOT/vendor/m-vave-fm1/app" -I"$ROOT/vendor/m-vave-fm1/app/synth" -I"$ROOT/vendor/m-vave-fm1/sdk/include" \
  "$ROOT/firmware/fm1lab_instrument.c" \
  "$ROOT/firmware/fm1lab_bank_metadata.c" \
  "$ROOT/firmware/fm1lab_library.c" \
  "$ROOT/tests/fm1lab_library.test.c" -o "$LIBRARY_BIN"
"$LIBRARY_BIN"
echo "FM1-Lab four-patch library integration tests passed."

cc -std=c11 -Wall -Wextra -Werror -I"$ROOT/firmware" -I"$ROOT/vendor/m-vave-fm1/app" -I"$ROOT/vendor/m-vave-fm1/app/synth" -I"$ROOT/vendor/m-vave-fm1/sdk/include" \
  "$ROOT/firmware/fm1lab_instrument.c" "$ROOT/firmware/fm1lab_bank_metadata.c" \
  "$ROOT/firmware/fm1lab_library.c" "$ROOT/firmware/fm1lab_runtime.c" \
  "$ROOT/tests/fm1lab_runtime.test.c" -o "$RUNTIME_BIN"
"$RUNTIME_BIN"
echo "FM1-Lab runtime control tests passed."

cc -std=c11 -Wall -Wextra -Werror \
  -I"$ROOT/firmware" -I"$ROOT/vendor/m-vave-fm1/app" -I"$ROOT/vendor/m-vave-fm1/app/synth" -I"$ROOT/vendor/m-vave-fm1/sdk/include" \
  "$ROOT/firmware/fm1lab_sequencer.c" "$ROOT/tests/fm1lab_sequencer.test.c" -o "$SEQUENCER_BIN"
"$SEQUENCER_BIN"
echo "FM1-Lab native sequencer adapter tests passed."

cc -std=c11 -Wall -Wextra -Werror -I"$ROOT/app" -I"$ROOT/app/dsp" \
  "$ROOT/tests/settings_record.test.c" -o "$SETTINGS_BIN"
"$SETTINGS_BIN"

cc -std=c11 -Wall -Wextra -Werror -I"$ROOT/firmware" \
  "$ROOT/firmware/fm1lab_instrument.c" "$ROOT/firmware/fm1lab_voice_editor.c" \
  "$ROOT/tests/fm1lab_voice_editor.test.c" -o "$SEQUENCER_BIN"
"$SEQUENCER_BIN"
echo "FM1-Lab native voice editor tests passed."

# Exercise the legacy M-VAVE SDK prototype engine, then the active Felucca FM6 device core.
bash "$ROOT/tests/test-audio-engine.sh"
bash "$ROOT/tests/test-fm6-engine.sh"

# The active app's compact 4-bank store is separate from the legacy SDK bank codec.
bash "$ROOT/tests/test-store-library.sh"
