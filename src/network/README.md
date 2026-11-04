# DirectPlay 4A probe

`dplay-probe.c` is a small x86 Windows console program for testing the DirectPlay 4A TCP/IP provider independently of Hogs of War. It uses a private probe GUID and does not load the game, create a game window, or access game files.

Build with Zig 0.14.1:

```sh
zig cc -target x86-windows-gnu -O2 dplay-probe.c -o dplay-probe.exe -lole32
```

In one terminal, start a host:

```text
dplay-probe.exe --host --timeout 30
```

In another terminal, join it by address:

```text
dplay-probe.exe --join 127.0.0.1 --timeout 30
```

Use the host's reachable address for a routed test. The host address element is empty; the joining process supplies the host address. Both processes must report success for the probe exchange to pass. That result checks DirectPlay messaging only; it does not prove that Hogs of War's menus, game sessions, Internet routing, or a particular compatibility layer works.

`--timeout` bounds the whole process, including COM/DirectPlay calls and cleanup (1..3600 seconds). If an API call or cleanup blocks past the limit, the probe writes a final `process_timeout` JSON record and terminates only itself with exit code 124. Join hosts may contain only ASCII letters, digits, dots, and hyphens.

The probe writes newline-delimited JSON progress records to stdout, beginning with role, PID, version, and timeout. Major COM/DirectPlay calls have `begin` and `end` records with elapsed milliseconds and HRESULT; release calls report their reference count, and receive loops are labeled by role. Failure is printed before cleanup starts, so a later cleanup hang does not hide the original failure. The enumeration callback stops immediately when DirectPlay reports `DPESC_TIMEDOUT`.

The probe relies on the Windows-compatible `dplay.h` and `dplobby.h` declarations. Its compound address follows the approach in [Wine's DirectPlay TCP/IP tests](https://github.com/wine-mirror/wine/blob/master/dlls/dplayx/tests/dplayx.c). Protocol reference: [Microsoft DirectPlay 4](https://learn.microsoft.com/en-us/openspecs/windows_protocols/mc-dpl4cs/10eeb2a2-da0e-4ce2-98dc-ba1a87092a68).

No Microsoft DirectPlay DLLs or redistributable are included here. Use the supported installer preparation flow for the selected platform; do not copy system DLLs from another computer.
