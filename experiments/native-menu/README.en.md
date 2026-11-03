# Staged native mouse/menu build

This is an isolated PE32 candidate built from the current HogsMouse input source plus the native Options-menu bridge, runtime, and DirectDraw renderer. It does not include game executables or game data.

The linear mode-14 aim detour is guarded by `Mouse/LinearAimEnable=0` and is off in the supplied INI. If explicitly enabled, it applies only to the verified mode-14 / weapon-7 path; other aim paths retain the existing digital handling. Its input packet uses a process-local token not decoded by remote peers, so this candidate is **not** multiplayer-compatible when linear aim is enabled and must stay out of friend/network installers.

## Build and checks

Run `bash build.sh` with Zig 0.14.1 on PATH (or set `ZIG` to its executable path). It compiles the release ASI and two PE32 fixtures. The script runs host-side aim, patch-policy, menu-policy, runtime-policy, and native-input-policy tests. The PE32 fixtures are compiled only; they are intended for the integration owner's isolated test prefix, never the live game.

The parent integration owner subsequently ran the PE32 axis/input fixtures and release-DLL load probe in an isolated Wine prefix; all passed. Native Options callbacks and save/cancel/error handling passed a separate Win32 fixture. Actual game visibility and physical acceptance remain unverified. No installed game or app file was changed.

This prototype targets the existing Russian localization and its custom bitmap-font alphabet. It is not a universal-language release. Original Options labels are fetched from FE17 and FE21–23, while the added labels use that Russian atlas. The Options screen itself is drawn through native glyphs so a fourth row does not depend on unverified legacy row caches.

The menu exposes the actual tuning keys. With linear aim disabled, increasing gain still reaches the original peak-speed limits. Do not claim that the menu alone fixes the owner’s sensitivity complaint. The pre-mission release-before-press guard applies only when the game enters its existing conditional wait phase; it does not force every mission to wait.
