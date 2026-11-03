# Building

Prebuilt test installers are available in GitHub Releases. The repository includes pinned renderer/plugin payloads and the 24-page texture profile; complete game archives, executables and account data are not build inputs.

## Windows installer

Install Zig 0.14.1 (or a compatible compiler), place `zig` on PATH, and run from this repository root:

```sh
sh work/multiplayer/windows/build.sh
python3 work/multiplayer/windows/verify_static.py
sh work/multiplayer/windows/build-profile-selftest.sh
```

The installer is written to `outputs/Hogs-of-War-Windows-Setup.exe`. The console-only transaction fixture is built under `work/multiplayer/windows/build`; run it on Windows or in an isolated Wine prefix. It creates its own temporary synthetic archive set and never launches Hogs of War. Exact inputs are in [DEPENDENCIES.md](work/multiplayer/windows/DEPENDENCIES.md).

## macOS installer

Requires macOS with Xcode Command Line Tools, `pkgbuild`, Python 3, and a Wine runtime seed app. Set `HOGS_WINE_APP` to an installed Hogs of War Online app from the project release, or to a compatible app bundle with the same Wine/Frameworks layout. The seed contributes runtime libraries and the icon, not the user's game data. Its runtime is Wine Sikarugir 10.0 revision 3 plus DXVK 1.10.3 async; see third-party sources below.

```sh
HOGS_WINE_APP='/Applications/Hogs of War Online.app' python3 work/multiplayer/mac/build.py
```

Output: `outputs/Hogs-of-War-Mac-Setup.pkg`. The builder clones the seed to its staging directory, compiles the native launcher, signs the staged app ad hoc and builds an unsigned package. It does not launch Wine or the game. A Developer ID certificate and notarization are not configured.

## Mouse candidate

The ASI targets 32-bit Windows, including the validated 32-bit Steam executable under Wine. From the repository root:

```sh
zig cc -target x86-windows-gnu -fms-extensions -std=c11 -O2 -Isrc/mouse -Wl,--strip-all -shared -o HogsMouse.asi src/mouse/HogsMouse.c -luser32 -lkernel32
zig cc -target x86-windows-gnu -fms-extensions -std=c11 -O2 -DHOGSMOUSE_TEST -Isrc/mouse -o free_aim_harness.exe src/mouse/HogsMouse.c tests/mouse/free_aim_harness.c -luser32 -lkernel32
cc -std=c11 -O2 -Isrc/mouse tests/mouse/free_aim_policy_test.c -o free_aim_policy_test
```

The candidate is not installed by the 0.5 release. On macOS it also requires the native focus publisher and matching launcher wiring described in `src/native/README.md`; copying only the ASI is insufficient for the complete Mac focus protection.

## Texture Toolkit adaptation

`outputs/HD-профиль Hogs of War/assets/combined-final-rgba-mutable.patch` contains our adaptation of the MIT-licensed Texture Toolkit. It captures the final RGBA atlas from the dgVoodoo path and restores original resources on mutable writes. The upstream repository and component licenses are listed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). The built x86 ASI is pinned to SHA-256 `203f27318705c0ef264e9835102bb329982dd1322807817a436bc42f49be8e53`.
