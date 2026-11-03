# HogsMouse native body input and live tuning

The owner accepted character turning with the chase camera in battle on
2026-11-04. The next revision corrects both weapon-aim axes at the owner's
request, adds nominal 3x sensitivity and reloadable tuning. These new settings
have console validation; their physical feel is pending the next owner test.

Mouse X uses the original synchronized input packet and native character turn.
RMB with a weapon uses the verified digital hold-to-aim path; LMB independently
fires, charges and releases. Unarmed RMB does not open independent orbit.
Free vertical view while walking remains unsupported. No camera/pig memory
is written. Actual two-computer multiplayer mouse acceptance is unverified.

`HogsMouse.ini` defaults: `NativeUnitsPerPixel=12` (range 1..24),
`AimSensitivityPercent=300` (25..1000), `InvertX=1`, `InvertY=1`.
Inversion affects weapon aiming only. Body gain was previously 4. The native
body axis remains capped at +/-120, so 3x applies below saturation. Digital
aim can emit at most one direction per read; scaling improves the response to
small movements without promising 3x peak angular velocity or queuing steps.

The settings file is polled at most once per second using metadata. Reloading
waits until physical and event-owned buttons are released, clears all staged
motion and disarms until release, and preserves the runtime enable toggle.
The separate Mac panel edits only sensitivity and inversion. Native frontend
menu insertion needs further verified reverse engineering and is not claimed.

The executable/input bridge, foreground publisher and HogsFix keyboard chain
remain version guarded. All 140 original model/texture archives remain intact,
with the existing 24-page GPU HD replacement profile.

## Build and validation

Use Zig 0.14.1, `-target x86-windows-gnu -fms-extensions -std=c11 -O2`,
`-Ihelpers`, `-shared`, `-luser32 -lkernel32` for `HogsMouse.c`. For the console
harness add `-DHOGSMOUSE_TEST`, omit `-shared`, add
`tests/native_input_harness.c`. Pure policy/scaler tests use ordinary C11.

Run Windows fixtures sequentially only in a separate test prefix. The runtime
record covers six bridge groups, nineteen production-input groups and a
release-ASI import/DllMain load probe. Pure tests cover fractional sensitivity,
reversal, bounds, reset, independent fire and native packet ownership. The
fixtures use synthetic memory and inputs; they do not launch the game.
