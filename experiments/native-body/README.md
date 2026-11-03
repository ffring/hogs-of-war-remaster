# Native body-turn experiment

This source is an experiment installed on the owner machine for a manual battle test. It is not included in the 0.5 test installers and has not been accepted in a real game. Installation/readback and launcher/profile checks passed with the game closed; no game was launched by the installer. No runtime logs or game executable are included here.

Mouse X is packed into the original synchronized input record and the game applies native pig turning. The original mode-4 camera follows the pig, as observed in the owner keyboard trace. Holding RMB with a selected weapon retains the already tested digital aim-key path; unarmed RMB does not request the independent orbit camera. LMB remains native fire/charge/release. Free vertical view outside weapon aiming is not implemented. Actual multiplayer mouse behavior is unverified.

The native bridge replaces only the version-checked seven-byte instruction at 0x45DB97 in process memory, preserves the original MOV, registers, flags and stack, then calls back to edit the axis/mask input record. It retains the existing HogsFix keyboard callback chain. Camera and character memory are never written by this module.

BODY packets use gain 4 by default (`NativeUnitsPerPixel`, range 1..8). Native turn processing divides X twice, so a one-pixel delta yields one angle unit at the default gain. The packet is bounded and consumed once. Native human input type 0 is checked before applying this policy; unknown/type-2 controllers are rejected. Existing joystick input is retained. Keyboard UP/DOWN are converted only when they must coexist with a mouse turn; the retail branch was executed offline to verify their sign and native walk arguments. Other action bits survive the conversion.

Thirteen production-input console groups, six bridge ABI/patch groups, the release ASI load check and native policy checks passed using synthetic input/memory. Sixteen additional executions fed real policy-generated packets through a bounded fragment of the verified retail executable, confirming one-pixel turns and simultaneous UP/DOWN walking for the human and synchronized-queue types. No game was launched for these checks. This does not establish physical mouse feel, weapon interaction quality or full multiplayer acceptance.

## Build

Use Zig 0.14.1 from the repository root:

```sh
zig cc -target x86-windows-gnu -fms-extensions -std=c11 -O2 -Iexperiments/native-body/helpers -Wl,--strip-all -shared -o HogsMouse-native-body.asi experiments/native-body/HogsMouse.c -luser32 -lkernel32
zig cc -target x86-windows-gnu -fms-extensions -std=c11 -O2 -DHOGSMOUSE_TEST -Iexperiments/native-body/helpers -o native_input_harness.exe experiments/native-body/HogsMouse.c experiments/native-body/tests/native_input_harness.c -luser32 -lkernel32
zig cc -target x86-windows-gnu -fms-extensions -std=c11 -O2 -Iexperiments/native-body/helpers -o native_axis_bridge_harness.exe experiments/native-body/tests/native_axis_bridge_harness.c -lkernel32
cc -std=c11 -O2 -Iexperiments/native-body/helpers experiments/native-body/tests/native_input_policy_test.c -o native_input_policy_test
```

Run Windows console fixtures only in a separate test prefix, without a game process. Mac runtime input also needs the native foreground publisher and matching launcher wiring in `src/native`. Installing only an ASI does not supply that integration. Close the game before replacing modules. Keep the complete current profile and rollback files together.
