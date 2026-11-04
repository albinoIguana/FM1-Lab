#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SOURCE="$ROOT/vendor/m-vave-fm1"
SDK_VERSION="27.0"
SDK_ARCHIVE="wasi-sdk-${SDK_VERSION}-x86_64-linux.tar.gz"
SDK_URL="https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-27/${SDK_ARCHIVE}"
SDK_PARENT="${RUNNER_TEMP:-$ROOT/.cache}"
SDK_ROOT="$SDK_PARENT/wasi-sdk-${SDK_VERSION}-x86_64-linux"
OUT="$SOURCE/build/web"

# Compile the FM1-Lab native Instrument and bank-metadata modules with the official app sources.
cp "$ROOT/firmware/fm1lab_instrument.h" "$SOURCE/app/fm1lab_instrument.h"
cp "$ROOT/firmware/fm1lab_instrument.c" "$SOURCE/app/fm1lab_instrument.c"
cp "$ROOT/firmware/fm1lab_library.h" "$SOURCE/app/fm1lab_library.h"
cp "$ROOT/firmware/fm1lab_library.c" "$SOURCE/app/fm1lab_library.c"
cp "$ROOT/firmware/fm1lab_runtime.h" "$SOURCE/app/fm1lab_runtime.h"
cp "$ROOT/firmware/fm1lab_runtime.c" "$SOURCE/app/fm1lab_runtime.c"
cp "$ROOT/firmware/fm1lab_sequencer.h" "$SOURCE/app/fm1lab_sequencer.h"
cp "$ROOT/firmware/fm1lab_sequencer.c" "$SOURCE/app/fm1lab_sequencer.c"
cp "$ROOT/firmware/fm1lab_voice_editor.h" "$SOURCE/app/fm1lab_voice_editor.h"
cp "$ROOT/firmware/fm1lab_voice_editor.c" "$SOURCE/app/fm1lab_voice_editor.c"
cp "$ROOT/firmware/fm1lab_bank_metadata.h" "$SOURCE/app/fm1lab_bank_metadata.h"
cp "$ROOT/firmware/fm1lab_bank_metadata.c" "$SOURCE/app/fm1lab_bank_metadata.c"
cp "$ROOT/firmware/fm1lab_physical_controls.h" "$SOURCE/app/fm1lab_physical_controls.h"
cp "$ROOT/firmware/fm1lab_audio_engine.h" "$SOURCE/app/fm1lab_audio_engine.h"
cp "$ROOT/firmware/fm1lab_audio_engine.c" "$SOURCE/app/fm1lab_audio_engine.c"
cp "$ROOT/firmware/fm1lab_audio_bridge.h" "$SOURCE/app/fm1lab_audio_bridge.h"
cp "$ROOT/firmware/fm1lab_audio_bridge.c" "$SOURCE/app/fm1lab_audio_bridge.c"
node "$ROOT/scripts/patch-m-vave-instrument-storage.mjs"
node "$ROOT/scripts/patch-m-vave-two-track-sequencer.mjs"
node "$ROOT/scripts/patch-m-vave-physical-controls.mjs"
node "$ROOT/scripts/patch-m-vave-instrument-home.mjs"
node "$ROOT/scripts/patch-m-vave-instrument-sequencer-screen.mjs"
node "$ROOT/scripts/patch-m-vave-native-patch-editor.mjs"
node "$ROOT/scripts/patch-m-vave-audio-mixer.mjs"
node "$ROOT/scripts/patch-m-vave-simulator-api.mjs"

if [[ ! -x "$SDK_ROOT/bin/clang" ]]; then
  mkdir -p "$SDK_PARENT"
  curl --fail --location --retry 2 "$SDK_URL" --output "$SDK_PARENT/$SDK_ARCHIVE"
  tar -xzf "$SDK_PARENT/$SDK_ARCHIVE" -C "$SDK_PARENT"
fi

mkdir -p "$OUT"
mapfile -t sources < <(find "$SOURCE/app" "$SOURCE/web" "$SOURCE/third_party/synth" \
  -type f -name '*.c' ! -path "$SOURCE/app/board/*" | sort)
if [[ ${#sources[@]} -eq 0 ]]; then
  echo "No M-VAVE browser engine C sources found." >&2
  exit 1
fi

includes=(
  "-I$SOURCE/web/engine"
  "-I$SOURCE/web/synth"
  "-I$SOURCE/third_party/synth"
  "-I$SOURCE/sdk/include"
  "-I$SOURCE/app/synth"
  "-I$SOURCE/app"
)

"$SDK_ROOT/bin/clang" "--sysroot=$SDK_ROOT/share/wasi-sysroot" \
  -std=c11 -O2 -fwrapv -fno-fast-math -ffp-contract=off \
  -DFM1_HOST_TEST -DFM1_OPEN_WEB -Wall -Wextra -Werror \
  -Wno-unused-function -Wno-array-parameter -mexec-model=reactor \
  "${includes[@]}" "${sources[@]}" \
  -Wl,--strip-all -Wl,-z,stack-size=131072 \
  -Wl,--initial-memory=4194304 -Wl,--max-memory=33554432 \
  -o "$OUT/simulator.wasm"

echo "Built official M-VAVE simulator: $OUT/simulator.wasm"
