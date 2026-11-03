#!/bin/sh
set -eu
ZIG=${ZIG:-zig}
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$HERE/.." && pwd)
OUT="$ROOT/build"
mkdir -p "$OUT"
"$ZIG" cc -target x86-windows-gnu -std=c11 -O2 -Wall -Wextra -Werror -Wno-unused-function -c -o "$OUT/native_ini_release.o" "$ROOT/release_compile.c"
"$ZIG" cc -target x86-windows-gnu -std=c11 -O2 -Wall -Wextra -Werror -Wno-unused-function -o "$OUT/native_ini_test.exe" "$HERE/native_ini_test.c" -lkernel32
printf '%s\n' "$OUT/native_ini_test.exe"
