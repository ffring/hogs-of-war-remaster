#!/bin/sh
set -eu
ROOT=$(cd "$(dirname "$0")/../../.." && pwd)
cd "$ROOT"
ZIG=${ZIG:-zig}
mkdir -p work/multiplayer/windows/build
"$ZIG" cc -target x86_64-windows-gnu -O2 -Wall -Wextra -c work/multiplayer/windows/gpu_profile_win.c -o work/multiplayer/windows/build/gpu_profile_win.o
"$ZIG" cc -target x86_64-windows-gnu -c work/multiplayer/windows/profile_payload.S -o work/multiplayer/windows/build/profile_payload.o
"$ZIG" cc -target x86_64-windows-gnu -O2 -Wall -Wextra -c work/multiplayer/windows/gpu_profile_selftest.c -o work/multiplayer/windows/build/gpu_profile_selftest.o
"$ZIG" cc -target x86_64-windows-gnu -O2 -Wl,--subsystem,console work/multiplayer/windows/build/gpu_profile_selftest.o work/multiplayer/windows/build/gpu_profile_win.o work/multiplayer/windows/build/profile_payload.o -lbcrypt -o work/multiplayer/windows/build/gpu-profile-selftest.exe
file work/multiplayer/windows/build/gpu-profile-selftest.exe
shasum -a 256 work/multiplayer/windows/build/gpu-profile-selftest.exe
