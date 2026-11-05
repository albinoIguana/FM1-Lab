#!/usr/bin/env sh
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO=$(CDPATH= cd -- "$HERE/.." && pwd)
cd "$REPO"
git submodule update --init --recursive firmware/base firmware/upstream/felucca
cd "$HERE"
python3 base/tools/build.py app "$@"
