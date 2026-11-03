# Online play

Both players need their own Steam copy of Hogs of War. The installers are distributed from the [project Releases page](https://github.com/ffring/hogs-of-war-remaster/releases); Tailscale is installed separately from its [official download page](https://tailscale.com/download).

## Prepare both computers

- **Mac:** install the Mac package, choose or download your Steam game files, then click **«Подготовить сетевой режим»**. This first step downloads Microsoft's legacy DirectPlay components on demand. When it finishes, click **«Играть по сети»**.
- **Windows:** install Hogs of War from Steam, select its game folder in the Windows setup program, and click **«Установить исправления»**. The HD profile is optional and is not required for networking. If Windows asks for DirectPlay, enable **Legacy Components → DirectPlay** in Windows Features ([Microsoft background](https://learn.microsoft.com/en-us/windows/compatibility/windows-components-installed-on-demand)).
- Install Tailscale on both computers and connect them to the same private tailnet using separate accounts. Do not share Steam or Tailscale passwords. Tailscale's [device-sharing model](https://tailscale.com/docs/reference/inviting-vs-sharing) is different from putting both computers on the same tailnet; use access controls appropriate for your network.

## Host and join

These steps assume the Mac is hosting. In Hogs of War, open **Multiplayer → Network → TCP/IP**. On the Mac host, leave the address field empty. On the joining computer, enter the host Mac's Tailscale IPv4 address. Do not expose game ports on a public router or disable the operating-system firewall.

The DirectPlay probe in [`src/network/dplay-probe.c`](../src/network/dplay-probe.c) checks DirectPlay 4A host/join messaging without launching the game. Its local two-process exchange has passed on one Mac. A full game match between two computers, Tailscale routing, and Mac-to-Windows interoperability have not yet been verified; treat this workflow as a test setup, not a confirmed multiplayer service.

## References

- [Microsoft DirectPlay 4 session and messaging protocol](https://learn.microsoft.com/en-us/openspecs/windows_protocols/mc-dpl4cs/10eeb2a2-da0e-4ce2-98dc-ba1a87092a68)
- [Wine DirectPlay TCP/IP tests](https://github.com/wine-mirror/wine/blob/master/dlls/dplayx/tests/dplayx.c)
- [Tailscale for Mac](https://tailscale.com/docs/install/mac) · [Tailscale for Windows](https://tailscale.com/docs/install/windows)
