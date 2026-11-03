#!/usr/bin/env bash
set -euo pipefail
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
OUT="$HERE/build"
ZIG=${ZIG:-zig}
CC=${CC:-cc}
mkdir -p "$OUT"

"$CC" -std=c11 -O2 -Wall -Wextra -Werror \
  "$HERE/tests/native_aim_wrapper_test.c" -o "$OUT/native_aim_wrapper_test"
"$OUT/native_aim_wrapper_test"
"$CC" -std=c11 -O2 -Wall -Wextra -Werror \
  "$HERE/tests/native_aim_patch_policy_test.c" -o "$OUT/native_aim_patch_policy_test"
"$OUT/native_aim_patch_policy_test"
"$CC" -std=c11 -O2 -Wall -Wextra -Werror \
  "$HERE/tests/menu_policy_test.c" -o "$OUT/menu_policy_test"
"$OUT/menu_policy_test"
"$CC" -std=c11 -O2 -Wall -Wextra -Werror \
  "$HERE/tests/runtime_core_test.c" -o "$OUT/runtime_core_test"
"$OUT/runtime_core_test"
"$CC" -std=c11 -O2 -Wall -Wextra -Werror -I "$HERE/helpers" \
  "$HERE/tests/native_input_policy_test.c" -o "$OUT/native_input_policy_test"
"$OUT/native_input_policy_test"

COMMON=(-target x86-windows-gnu -fms-extensions -fasm-blocks -std=c11 -O2 -Wall -Wextra -Werror -Wno-unused-function)
INCLUDES=("-I$HERE" "-I$HERE/helpers" "-I$HERE/native-menu-v1" "-I$HERE/native-menu-policy-v1" "-I$HERE/native-runtime-v1" "-I$HERE/native-render-v1" "-I$HERE/native-ini-v1" "-I$HERE/loading-prompt-v1")
# shellcheck disable=SC2086
"$ZIG" cc "${COMMON[@]}" "${INCLUDES[@]}" -o "$OUT/HogsMouse-native-menu.asi" \
  -shared -Wl,--strip-all \
  "$HERE/HogsMouse.c" \
  "$HERE/native-menu-v1/menu_bridge.c" \
  "$HERE/native-runtime-v1/native_runtime.c" \
  "$HERE/native-render-v1/native_render.c" \
  -luser32 -lkernel32 -lgdi32 -lddraw

# The bridge harness is compiled only; it must be run later in its isolated
# prefix by the parent integration owner, never against the live game.
# shellcheck disable=SC2086
"$ZIG" cc "${COMMON[@]}" "${INCLUDES[@]}" -Wl,--image-base,0x10000000 \
  -o "$OUT/native_axis_bridge_harness.exe" \
  "$HERE/tests/native_axis_bridge_harness.c" -lkernel32

# Input CUI fixture uses the same HogsMouse.c compiled with test-only seams.
# It is PE32 and intended for an isolated prefix only, never the game process.
# shellcheck disable=SC2086
"$ZIG" cc "${COMMON[@]}" "${INCLUDES[@]}" -Wno-unused-variable -DHOGSMOUSE_TEST \
  -Wl,--image-base,0x10000000 -o "$OUT/native_input_harness.exe" \
  "$HERE/HogsMouse.c" "$HERE/tests/native_input_harness.c" -luser32 -lkernel32

shasum -a 256 "$OUT/HogsMouse-native-menu.asi" \
  "$OUT/native_axis_bridge_harness.exe" "$OUT/native_input_harness.exe"
