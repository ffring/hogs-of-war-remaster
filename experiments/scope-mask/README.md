# Wider scope-mask candidate

The owner's original SIGHT.MAD is a flat opaque mask mesh with a geometric
circular aperture. Its SIGHT.MTD is a uniformly dark 4x4 material. All 24 HD page
alpha channels match their captured inputs, so no alpha change was made.
A Hor+ viewport can expose the finite outer mesh edges; the reported rectangle
has not yet been tied to a captured draw call. This is a candidate for that issue,
not a visually verified fix.

`build_sight_candidate.py INPUT_MAD OUTPUT_MAD` derives a separate 2x outer mask
from a user-owned original. It finds boundary loops and extends only the four
vertices on the larger outer boundary. The 42-vertex aperture, triangle topology,
UV/material blobs, archive directory and byte size remain unchanged. Four
structural tests pass with the supported original. Do not overwrite the original.
No game artwork or executable is included here.

Original SHA256: `0f5de9da73dc72f1d672549e19c2cda381d671c4606098cbd669b0ab1050ed9a`.
Derived SHA256: `f61586df40f18419c4261799ff96be903146a29b0838927d961acbdecff35a1a`.

The separate `runtime/HogsSightPath.asi.c` requires the exact supported Steam PE
identity, original path bytes at VA0x4CF974 and PUSH at VA0x4549AD. It validates
`hogs/sight.mad` (2672 bytes, CRC32 B489FD73) and matching CWD before replacing
only the in-memory filename string with `hogs\sight.mad`. It retains the original
path on any guard failure. SIGHT.MTD is still loaded from its independent original
path. No executable file, original MAD/MTD or camera state is modified.

The runtime uses only Kernel32 imports and shared C-ABI protection adapters.
Ten synthetic/real-Win32-memory fixture groups and release-DLL load passed in a
separate Wine prefix. This establishes guarded string patch behavior; actual
asset binding, load timing and widescreen visual acceptance remain pending.
The ASI appends one status line to `HogsSightPath.log` beside the game executable.

Build with Zig 0.14.1, x86-windows-gnu, C11, -O2 -Wall -Wextra -Werror,
-nostdlib, -shared, -Wl,-e,DllMain@12, -lkernel32, Windows system headers and
`-Iexperiments/scope-mask/runtime`. The console harness uses the same headers,
normal CRT linkage and `-lkernel32`. Run it only in an isolated test prefix.
Close the game before installing either alias or ASI. Keep this experimental
component out of default installers until the owner has accepted its visual result.
