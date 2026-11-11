#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
CC="${CC:-cc}"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
"$CC" -std=c99 -Wall -Wextra -Werror -O2 tests/flash_wait_test.c -o "$TMP/flash_wait_test"
"$TMP/flash_wait_test"
"$CC" -std=c99 -Wall -Wextra -Werror -O2 tests/flash_chip_ids_test.c -o "$TMP/flash_chip_ids_test"
"$TMP/flash_chip_ids_test"
python3 -m unittest discover -s tests -p 'test_flash_rescue_ids.py'
python3 tests/flash_nor_model.py
python3 tools/apply-fm1-flash-compat.py --check
