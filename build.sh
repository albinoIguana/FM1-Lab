#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$ROOT"
git submodule update --init --recursive base
python3 base/tools/build.py app "$@"
