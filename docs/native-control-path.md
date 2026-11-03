# Native input research — incomplete implementation

Target: Steam November 2000 PE32 executable, SHA-256 `32eb8c28b383d532091dcae2057d98593be4dc064f2117212a65ced41509e246`. This document records observations, not a working chase-camera patch. No game executable is distributed here.

The existing mouse module injects configured DirectInput keys through the saved HogsFix callback. That proves button delivery but inherits the original context-dependent camera commands.

Static tracing located a native analog route:

1. Human input method `0x45D700` builds the button mask and packed axis output.
2. Event 4 enters the synchronized control queue. Its transport also uses DirectPlay; this is not solely a network-only path.
3. Controller enqueue `0x48CE70` stores mask, axes and timestamp. Pop `0x48CE00` returns the mask and axis output.
4. Battle update copies these into state fields `+0x440` (mask) and `+0x444` (packed axes).
5. Native camera handler `0x495690` consumes signed axis bytes with mode-specific behavior. The first two axis bytes are scaled toward zero by two; additional packed bytes and previous input fields are also used.

Camera modes matter. `0x49F650` changes `camera+0x78`. In mode 0, `0x49F470` changes the distance-like field `+0xB4`; in modes 4 and 18 it changes `+0x76`. In mode 18 with a selected weapon, horizontal input can be routed to weapon aiming instead of ordinary camera yaw. L1 is a separate action in the handler, not a universal hold gate for analog input.

Thirty-four bounded CPU emulations of the two camera methods used synthetic camera memory, the verified PE sections, and a sentinel return. All returned without invoking the game entry point, imports, Wine or graphics. These tests established field changes, signs and directional bounds. The original bounds check the prior value in several branches; they are not universal post-update clamps.

Pig facing at `object+0x174` and camera angles at `+0x88/+0x8C/+0x90` are signed 32-bit integer angles. Pig facing participates in movement calculations. Automatic behind-the-back camera following has not been established. Writing those fields blindly would bypass the synchronized input path and may conflict with render interpolation.

The target remains ordinary mouse controls: view and character turning together, a camera centered behind the character, held RMB aiming and independent LMB fire/charge. Native-axis injection, camera/character following and multiplayer compatibility still require implementation and actual game acceptance. The released 0.5 installers remain unchanged.

Relevant upstream source: [HogsFix](https://github.com/Torisutan/HogsFix/tree/211b06480d6f5bfb11a62e5367549cb684a7de4c), whose gamepad and interpolation code provide the existing integration boundary.
