#!/bin/sh
set -eu
ROOT=$(cd "$(dirname "$0")/../../.." && pwd)
cd "$ROOT"
ZIG=${ZIG:-zig}
mkdir -p work/multiplayer/windows/build outputs
"$ZIG" cc -target x86_64-windows-gnu -O2 -Wall -Wextra -c work/multiplayer/windows/installer.c -o work/multiplayer/windows/build/installer.o
"$ZIG" cc -target x86_64-windows-gnu -O2 -Wall -Wextra -c work/multiplayer/windows/gpu_profile_win.c -o work/multiplayer/windows/build/gpu_profile_win.o
"$ZIG" cc -target x86_64-windows-gnu -c work/multiplayer/windows/payload.S -o work/multiplayer/windows/build/payload.o
"$ZIG" cc -target x86_64-windows-gnu -O2 -Wl,--subsystem,windows work/multiplayer/windows/build/installer.o work/multiplayer/windows/build/gpu_profile_win.o work/multiplayer/windows/build/payload.o -lcomctl32 -lshell32 -lole32 -lbcrypt -o outputs/Hogs-of-War-Windows-Setup.exe
file outputs/Hogs-of-War-Windows-Setup.exe
shasum -a 256 outputs/Hogs-of-War-Windows-Setup.exe
