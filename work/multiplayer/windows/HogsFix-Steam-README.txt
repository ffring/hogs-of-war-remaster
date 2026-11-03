Hogs of War — Windows setup notes

This package supports the Steam PC release with Steam app ID 389900. It installs HogsFix and the Ultimate ASI Loader next to warhogs_.exe. The HogsFix plugin is supplied under its upstream license. Keep the included license file with it.

The optional HD profile adds Texture Toolkit and 24 captured texture pages. It uses the included unmodified 32-bit DDraw.dll and D3DImm.dll from dgVoodoo2 2.79.3; Windows supplies Direct3D 11. The profile has not yet been verified in a complete Windows battle. It does not change the game executable or its 140 MTD/MAD archives.

Close the game before installing or restoring files. The setup keeps a backup of files it replaces. Use its restore buttons before verifying or repairing the Steam installation.

DirectPlay is a Windows legacy optional component. If the game asks for it, enable it in Windows Features under Legacy Components > DirectPlay. For internet play, install Tailscale separately on both computers and use the peer's Tailscale address in the game's TCP/IP multiplayer screen. A complete two-computer game has not yet been verified.
