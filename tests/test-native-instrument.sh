#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"

# The active app must not compile the retired M-VAVE 16-recipe factory path.
if grep -Eq "fm1lab_factory_voice_build|fm1lab_factory_names|NEON KEYS" "$ROOT/app/app.c"; then
  echo "retired M-VAVE factory recipe source is still present in app/app.c" >&2
  exit 1
fi
INSTRUMENT_BIN="$(mktemp)"
METADATA_BIN="$(mktemp)"
CONTROLS_BIN="$(mktemp)"
LIBRARY_BIN="$(mktemp)"
RUNTIME_BIN="$(mktemp)"
SEQUENCER_BIN="$(mktemp)"
SETTINGS_BIN="$(mktemp)"
PRESET_BIN="$(mktemp)"
APP_INSTRUMENT_BIN="$(mktemp)"
EVENT_ORDER_BIN="$(mktemp)"
PENDING_WATCH_BIN="$(mktemp)"
AUDIO_PEAK_BIN="$(mktemp)"
CPU2_STARTUP_BIN="$(mktemp)"
trap 'rm -f "$INSTRUMENT_BIN" "$METADATA_BIN" "$CONTROLS_BIN" "$LIBRARY_BIN" "$RUNTIME_BIN" "$SEQUENCER_BIN" "$SETTINGS_BIN" "$PRESET_BIN" "$APP_INSTRUMENT_BIN" "$EVENT_ORDER_BIN" "$PENDING_WATCH_BIN" "$AUDIO_PEAK_BIN" "$CPU2_STARTUP_BIN"' EXIT
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

# Exercise the shared route + action dispatcher used by app.c, including the
# HOME focused-patch/whole-Instrument split and SEQ SELECT/PRESETS separation.
cc -std=c11 -Wall -Wextra -Werror -I"$ROOT/firmware" \
  "$ROOT/tests/fm1lab_input_route.test.c" -o "$CONTROLS_BIN"
"$CONTROLS_BIN"
echo "FM1-Lab app input routing and action-dispatch regression tests passed."

cc -std=c11 -Wall -Wextra -Werror -I"$ROOT/app" \
  "$ROOT/tests/fm1lab_preset_control.test.c" -o "$PRESET_BIN"
"$PRESET_BIN"
echo "FM1-Lab Instrument/patch preset focus tests passed."

cc -std=c11 -Wall -Wextra -Werror \
  "$ROOT/app/fm1lab_instrument.c" "$ROOT/tests/app_fm1lab_instrument.test.c" \
  -o "$APP_INSTRUMENT_BIN"
"$APP_INSTRUMENT_BIN"
echo "FM1-Lab app-side patch replacement isolation tests passed."

cc -std=c11 -Wall -Wextra -Werror "$ROOT/tests/fm1lab_event_order.test.c" -o "$EVENT_ORDER_BIN"
"$EVENT_ORDER_BIN"
echo "FM1-Lab event FIFO/heavy-budget property tests passed."

cc -std=c11 -Wall -Wextra -Werror "$ROOT/tests/fm1lab_pending_watch.test.c" -o "$PENDING_WATCH_BIN"
"$PENDING_WATCH_BIN"
echo "FM1-Lab pending-load stall watchdog tests passed."

cc -std=c11 -Wall -Wextra -Werror "$ROOT/tests/fm1lab_audio_peak_window.test.c" -o "$AUDIO_PEAK_BIN"
"$AUDIO_PEAK_BIN"
echo "FM1-Lab audio peak-window tests passed."

cc -std=c11 -Wall -Wextra -Werror "$ROOT/tests/fm1lab_cpu2_startup.test.c" -o "$CPU2_STARTUP_BIN"
"$CPU2_STARTUP_BIN"
echo "FM1-Lab CPU2 startup self-test tests passed."

cc -std=c11 -Wall -Wextra -Werror -DFM1_HOST_TEST \
  -I"$ROOT/firmware" -I"$ROOT/vendor/m-vave-fm1/app" -I"$ROOT/vendor/m-vave-fm1/app/synth" -I"$ROOT/vendor/m-vave-fm1/sdk/include" \
  "$ROOT/firmware/fm1lab_instrument.c" \
  "$ROOT/firmware/fm1lab_bank_metadata.c" \
  "$ROOT/firmware/fm1lab_library.c" \
  "$ROOT/tests/fm1lab_library.test.c" -o "$LIBRARY_BIN"
"$LIBRARY_BIN"
echo "FM1-Lab four-patch library integration tests passed."

cc -std=c11 -Wall -Wextra -Werror -DFM1_HOST_TEST -I"$ROOT/firmware" -I"$ROOT/vendor/m-vave-fm1/app" -I"$ROOT/vendor/m-vave-fm1/app/synth" -I"$ROOT/vendor/m-vave-fm1/sdk/include" \
  "$ROOT/firmware/fm1lab_instrument.c" "$ROOT/firmware/fm1lab_bank_metadata.c" \
  "$ROOT/firmware/fm1lab_library.c" "$ROOT/firmware/fm1lab_runtime.c" \
  "$ROOT/tests/fm1lab_runtime.test.c" -o "$RUNTIME_BIN"
"$RUNTIME_BIN"
echo "FM1-Lab runtime control tests passed."

cc -std=c11 -Wall -Wextra -Werror -DFM1_HOST_TEST \
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

# Exercise the exact bundled DX7 decoder and patch replacement used by app.c.
cc -std=c11 -Wall -Wextra -Werror -I"$ROOT/app" \
  "$ROOT/app/fm1lab_instrument.c" "$ROOT/app/fm1lab_dx7_preset.c" \
  "$ROOT/tests/fm1lab_dx7_preset.test.c" -o "$PRESET_BIN"
"$PRESET_BIN"
echo "FM1-Lab bundled DX7 preset decode/replacement tests passed."

# Exercise the legacy M-VAVE SDK prototype engine, then the active Felucca FM6 device core.
bash "$ROOT/tests/test-audio-engine.sh"
bash "$ROOT/tests/test-fm6-engine.sh"

# The active app's compact 4-bank store is separate from the legacy SDK bank codec.
bash "$ROOT/tests/test-store-library.sh"

# Run fm1-base's native test against the exact pinned production storage.c pool implementation.
cc -std=c11 -Wall -Wextra -Werror -Wno-unused-function "$ROOT/base/tests/storage_pool_test.c" -o "$CONTROLS_BIN"
"$CONTROLS_BIN"

# Exercise the pinned storage.c pool with FM1-Lab's exact object IDs and 12-sector layout.
cc -std=c11 -Wall -Wextra -Werror -Wno-unused-function -DFM1_HOST_TEST \
  -I"$ROOT/app" -I"$ROOT/app/dsp" -I"$ROOT/base/include" \
  "$ROOT/app/fm1lab_instrument.c" "$ROOT/app/fm1lab_store_library.c" \
  "$ROOT/tests/fm1lab_flash_pool.test.c" -o "$CONTROLS_BIN"
"$CONTROLS_BIN"
