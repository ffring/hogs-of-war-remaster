> Legacy source: the owner requested settings in the game. See experiments/native-menu for the staged native replacement. This panel is no longer offered.

# Mac mouse settings panel

Native AppKit panel for the owner-installed HogsMouse live-tuning module.
This is a separate application, not a native menu insertion. It edits only
`NativeUnitsPerPixel`, `AimSensitivityPercent`, `InvertX` and `InvertY` in
`~/Library/Application Support/Hogs of War/prefix/drive_c/HogsofWar/HogsMouse.ini`.
Other settings/comments are retained. The corresponding mouse module reloads
a changed file at most once per second after all mouse buttons are released.

Sliders span 0.25x..6x. Defaults are 3x for both controls and both aim inversions
on, matching the current owner request. Inversion does not affect body turning.

Build on macOS with Xcode Command Line Tools: `sh src/mac-settings/build.sh`.
Run the resulting app binary with `--self-test` for nine isolated temporary-file
checks. Those do not touch production configuration or open a window. Normal
launch opens the panel; applying settings writes the fixed owner path atomically
and reads it back. Invalid/malformed files and symlink paths are refused.

The UI and error layouts were inspected using hidden AppKit rendering, without
foreground activation. A dark-background/light-appearance contrast defect and
a misleading unconditional rollback claim were fixed during that review.
The app is ad-hoc signed, not notarized. Installers 0.5 do not contain this panel.
