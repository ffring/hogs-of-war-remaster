# macOS native helpers

These sources support the Mac launcher and the optional foreground-focus gate used by mouse input. They do not read global mouse or keyboard events. The publisher reports only whether the Hogs process is the frontmost macOS application; the game-side module reads that small shared record before accepting mouse input.

The shared record layout and version are declared in [`focus_mailbox.h`](focus_mailbox.h). The launcher creates it with a per-launch token, starts `hogs-focus-publisher`, and then replaces itself with Wine so the publisher can follow the same process lifetime. `hogs-online-launcher-focus.c` retains the online launcher's DirectPlay and Winsock overrides while starting the same focus publisher.

Build the native publisher and launcher on macOS with the Apple Clang toolchain:

```sh
clang -arch arm64 -O2 -I . hogs-focus-publisher.m hogs_mac_focus.m -framework AppKit -o hogs-focus-publisher
clang -arch arm64 -O2 -I . hogs-launcher-focus.c -o hogs-launcher
clang -arch arm64 -O2 -I . hogs-online-launcher-focus.c -o hogs-online-launcher
```

The launchers expect the application bundle and game data in the locations described in the installer documentation. Run `--check` to check files without starting the game. These sources do not include Wine, Hogs of War, or Microsoft DirectPlay redistributables.
