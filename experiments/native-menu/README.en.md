# Native mouse controls and in-game settings, v2

Owner-local target: the pinned November 2000 Steam PE32 with the current Russian localization. No game executable or full game archive is included.

The local body-yaw hook changes only the argument of the existing native pig turn command at 492D9A; its original method and return address remain intact. At NativeUnitsPerPixel=12 the effective range is three times the original gain-4 baseline, including the previous peak-speed limit (30 to 90 units). Fractional low-gain input is carried without idle motion or reversal backlog. Ordinary aiming in camera modes14/17 uses native yaw and pitch commands; weapon IDs11/64 retain their original scope/zoom handling.

LinearAimEnable=1 enables these changes only when the verified DirectPlay session byte at manager+4AD is clear. Online, missing-manager and unreadable-state cases retain native synchronized input. A 16-entry process-local token queue matches actor, human user, pig, camera, weapon, mode, wire axes and route; each sample is used once and expires after100ms. Joystick ownership, keyboard directions, focus loss and session transitions invalidate conflicting local samples.

The original Options screen receives a fourth Mouse item and a native bitmap-font settings pane. Turn/aim sensitivity and both inversion axes are saved transactionally, then applied after mouse-button release. Original Audio, Controls and Credits actions remain native. The conditional mission wait requires release before a fresh keypress. Bounded events record menu open/save/cancel and wait enter/accept/leave in HogsMouse.log.

The source and release were tested with synthetic game memory, actual retail instruction emulation and isolated Wine console fixtures. The physical game was not launched for this upgrade. In-game visual acceptance and an actual two-computer network match remain unverified.

Build with Zig0.14.1 using bash build.sh. Set ZIG to its executable path if needed. The ASI is built without HOGSMOUSE_TEST; all fake-world and ABI test seams are absent from the release.
