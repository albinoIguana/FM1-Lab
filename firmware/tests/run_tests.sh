#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
cd "$ROOT"
GEN=firmware/app/build/gen
test -s "$GEN/felucca_tables.h"
test -s "$GEN/felucca_fm6.h"
mkdir -p firmware/app/build
cc -std=c99 -O2 -Wall -Wextra \
  -Ifirmware/app \
  -Ifirmware/app/dsp \
  -I"$GEN" \
  -Ifirmware/upstream/felucca/firmware/engines/src \
  firmware/tests/fm6_test.c firmware/app/dsp/fm6.c firmware/app/dsp/seq.c firmware/app/storage_format.c \
  -o firmware/app/build/fm6_host_test
firmware/app/build/fm6_host_test
