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

## Current mouse module and native settings

The owner-installed ASI targets the pinned 32-bit Steam executable and current Russian font assets. Build its self-contained source with Zig 0.14.1 and a host C compiler:

```sh
bash experiments/native-menu/build.sh
bash experiments/native-menu/native-runtime-v1/tests/build.sh
bash experiments/native-menu/native-ini-v1/tests/build.sh
```

The first script runs portable policy checks and writes `experiments/native-menu/build/HogsMouse-native-menu.asi` plus console fixtures. It does not launch Wine or the game. Run the Windows console fixtures only on Windows or in a separate isolated Wine prefix. [Validation and limitations](experiments/native-menu/VALIDATION.json) distinguish these checks from actual game acceptance.

The current release ASI hash is `225d7a31ea82fa5b4f88139709194a940252598739dbf272b80207de48afd19d`. It includes native body turning, ordinary weapon aiming and in-game Options → Mouse settings. Local enhancements are disabled in active or unknown network sessions; native synchronized input remains in use there. The 0.5 installers do not include this module.

On macOS it also requires the focus publisher and matching launcher wiring described in `src/native/README.md`; copying only the ASI is insufficient for the complete Mac focus protection. `src/mouse`, `tests/mouse` and `experiments/native-body` retain historical revisions for research; they are not the current module.

## Texture Toolkit adaptation

`outputs/HD-профиль Hogs of War/assets/combined-final-rgba-mutable.patch` contains our adaptation of the MIT-licensed Texture Toolkit. It captures the final RGBA atlas from the dgVoodoo path and restores original resources on mutable writes. The upstream repository and component licenses are listed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). The built x86 ASI is pinned to SHA-256 `203f27318705c0ef264e9835102bb329982dd1322807817a436bc42f49be8e53`.
