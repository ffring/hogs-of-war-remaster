# Third-party notices

This fan project does not claim ownership of Hogs of War, its trademarks, original models, artwork, executable or complete game archives. The 24 DDS files are modified game-derived texture pages for use with a legitimately owned game. They are not covered by the code license below and no independent rights to the underlying artwork are granted.

Project-authored launcher/installer/probe code is provided under the root MIT license. Existing attribution and licenses in derived files take precedence; this is not a relicensing of third-party components.

- **HogsFix** — [Torisutan/HogsFix](https://github.com/Torisutan/HogsFix). MIT; its license is included at `third_party/HogsFix/LICENSE` and `work/HogsFix/LICENSE`. Corresponding source is included under `third_party/HogsFix`; its original notices are retained. The mouse component derives from its input conventions and carries its MIT attribution in `src/mouse/LICENSE.txt`.
- **Ultimate ASI Loader** — [ThirteenAG/Ultimate-ASI-Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader), MIT. License: `work/HogsFix/LICENSE-UltimateASILoader.txt`.
- **Texture Toolkit** — [BadassBaboon/Texture-Toolkit](https://github.com/BadassBaboon/Texture-Toolkit), MIT. License and source adaptation patch: `outputs/HD-профиль Hogs of War/assets/`.
- **dgVoodoo 2.79.3** — [official project](https://dege.freeweb.hu/dgVoodoo2/), freeware under its own terms. Only unmodified x86 DDraw.dll and D3DImm.dll are supplied for this game-specific modification. Notice: `work/multiplayer/windows/DgVoodoo-Redistribution.txt`; [upstream redistribution terms](https://dege.freeweb.hu/dgVoodoo2/ReadmeGeneral/).
- **DXVK / DXVK-Sikarugir 1.10.3 async** — [upstream source](https://github.com/doitsujin/dxvk), [Sikarugir build project](https://github.com/Sikarugir-App/DirectX). License: `work/multiplayer/mac/third-party/DXVK-LICENSE.txt`.
- **Wine Sikarugir 10.0 revision 3** — [engine releases](https://github.com/Sikarugir-App/Engines/releases/tag/v1.0), [corresponding Wine source](https://github.com/Gcenx/winecx), [Wine upstream](https://gitlab.winehq.org/wine/wine). Wine is LGPL; license: `work/multiplayer/mac/third-party/Wine-LICENSE.txt`. The macOS installer also retains its runtime's bundled notices. No Apple GPTK/D3DMetal runtime is supplied by this project.
- **Microsoft DirectPlay** — not bundled as loose Microsoft DLLs. The macOS network preparation helper downloads a hash-pinned original DirectX redistributable on the user's request, presents its terms and extracts the required x86 components locally. The included Russian DirectX EULA is in `work/multiplayer/mac/third-party/`.
- **Tailscale** — installed separately from [the official downloads](https://tailscale.com/download); not bundled in our game installers.

Hashes identify the tested binary inputs; they do not replace upstream license terms. See the Windows dependency manifest and the texture profile manifest for individual payload hashes.
