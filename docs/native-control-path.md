# Native input research — accepted body turn, staged proportional aim

Target: Steam November 2000 PE32 executable, SHA-256 `32eb8c28b383d532091dcae2057d98593be4dc064f2117212a65ced41509e246`. Native body turn and following camera have since been accepted by the owner. The new proportional aim path remains staged. No game executable is distributed here.

The older src/mouse module injects configured DirectInput keys through the saved HogsFix callback. The installed experiments/native-body revision additionally publishes native synchronized body axes; aiming in that installed revision still uses digital direction commands.

Static tracing located a native analog route:

1. Human input method `0x45D700` builds the button mask and packed axis output.
2. Event 4 enters the synchronized control queue. Its transport also uses DirectPlay; this is not solely a network-only path.
3. Controller enqueue `0x48CE70` stores mask, axes and timestamp. Pop `0x48CE00` returns the mask and axis output.
4. Battle update copies these into state fields `+0x440` (mask) and `+0x444` (packed axes).
5. Native camera handler `0x495690` consumes signed axis bytes with mode-specific behavior. The first two axis bytes are scaled toward zero by two; additional packed bytes and previous input fields are also used.

Camera modes matter. `0x49F650` changes `camera+0x78`. In mode 0, `0x49F470` changes the distance-like field `+0xB4`; in modes 4 and 18 it changes `+0x76`. In mode 18 with a selected weapon, horizontal input can be routed to weapon aiming instead of ordinary camera yaw. L1 is a separate action in the handler, not a universal hold gate for analog input.

Thirty-four bounded CPU emulations of the two camera methods used synthetic camera memory, the verified PE sections, and a sentinel return. All returned without invoking the game entry point, imports, Wine or graphics. These tests established field changes, signs and directional bounds. The original bounds check the prior value in several branches; they are not universal post-update clamps.

Pig facing at `object+0x174` and camera angles at `+0x88/+0x8C/+0x90` are signed 32-bit integer angles. Pig facing participates in movement calculations. The original body-yaw input route has been accepted in a real battle: the pig turns and the existing camera follows behind it. Writing those fields blindly would bypass the synchronized input path and may conflict with render interpolation.

The target remains ordinary mouse controls: view and character turning together, a camera centered behind the character, held RMB aiming and independent LMB fire/charge. Body-axis injection and camera/character following have owner acceptance. Proportional aim and multiplayer mouse compatibility still require actual game acceptance. The released 0.5 installers remain unchanged.

Relevant upstream source: [HogsFix](https://github.com/Torisutan/HogsFix/tree/211b06480d6f5bfb11a62e5367549cb684a7de4c), whose gamepad and interpolation code provide the existing integration boundary.

## Corrected weapon-aim dispatch

An earlier source comment incorrectly said mode 14 did not consume analog axes.
The supported PE loads the mode without an index bias, reads the byte map at
`0x496514`, then the target table at `0x4964F4`. The actual map is:

- mode 14: class 4 → `0x495B29`;
- mode 17: class 5 → `0x495B29`;
- mode 18: class 6 → `0x495752`;
- mode 4: class 7 → common return.

`0x495B29` contains both digital and analog weapon aim. Its original analog
consumer has joystick acceleration and prior-input counters, so enabling the
packet alone is not a linear mouse fix. The new staged candidate uses verified
native pitch `0x46A7F0` and pig-vtable yaw `0x46AF30` while retaining the original
handler's other processing. It does not write camera angles directly.

The installed tuning reads 300%, but digital aim retains the original maximum
turning speed, and body-axis input still has its old consumer cap. The owner
reported the nominal sensitivity change was not perceptible. Do not describe
that installed tuning as a proven threefold increase in peak turning speed.
