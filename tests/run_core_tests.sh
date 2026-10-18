#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${TMPDIR:-/tmp}/alphasplit_core_tests"
g++ -std=c++17 -Wall -Wextra -Werror -O2 -fsanitize=address,undefined -fno-omit-frame-pointer \
  "$ROOT/src/AlphaSplitCore.cpp" "$ROOT/tests/test_core.cpp" -o "$OUT"
"$OUT"
