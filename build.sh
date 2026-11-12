#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$ROOT"
git submodule update --init --recursive base upstream/felucca
python3 tools/apply-fm1-base-audio-deadline.py
python3 base/tools/build.py app "$@"
