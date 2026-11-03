#!/bin/sh
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$HERE/.." && pwd)
OUT="$ROOT/build"
ZIG=${ZIG:-zig}
mkdir -p "$OUT"
cc -std=c11 -Wall -Wextra -Werror -O2 \
  "$HERE/runtime_core_test.c" -o "$OUT/runtime_core_test"
"$OUT/runtime_core_test"
"$ZIG" cc -target x86-windows-gnu -std=c11 -O2 -Wall -Wextra -Werror \
  -Wno-unused-function -c -o "$OUT/native_runtime.o" "$ROOT/native_runtime.c"
"$ZIG" cc -target x86-windows-gnu -std=c11 -O2 -Wall -Wextra -Werror \
  -Wno-unused-function -DHNR_TEST=1 -Wl,--image-base,0x10000000 \
  -DHNR_OPTIONS_ROOT_GLOBAL=0x20000AE0u -DHNR_CURRENT_NODE_GLOBAL=0x20000AD8u \
  -DHNR_FRONTEND_GLOBAL=0x20006120u -DHNR_ORIGINAL_ACTION_TABLE=0x21000FD8u \
  -DHNR_ORIGINAL_TARGET_TABLE=0x21001230u \
  -o "$OUT/runtime_win32_test.exe" "$HERE/runtime_win32_test.c" -lkernel32
printf '%s\n' "$OUT/native_runtime.o"
printf '%s\n' "$OUT/runtime_win32_test.exe"
