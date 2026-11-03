# Windows setup build inputs

`build.sh` and `build-profile-selftest.sh` run from the repository root. Install Zig with the `x86_64-windows-gnu` C target available, then set `ZIG` if the executable is not named `zig` on `PATH`.

The payload assembly embeds these project artifacts. Their exact SHA-256 values are recorded here so a publisher can copy and verify the required files without including private game data or build outputs.

| Repository path | SHA-256 | Used for |
|---|---|---|
| `work/HogsFix/dinput.dll` | `d5a059aa467a7a7127c8f6169f79fa63ff0f55986ee9eb2fd9a281bebf2aa2e6` | Ultimate ASI Loader |
| `work/HogsFix/HogsFix.asi` | `68d73e0803fb62edb739539cf3d78c41e355e2e5030acb7b056c409b49a96e3d` | Game fixes |
| `work/HogsFix/HogsFix.ini` | `0c8675888447bf512b7e7620ce258039a5fd51301a7a448d58e4be3bd79f9491` | Stable settings |
| `work/HogsFix/LICENSE-UltimateASILoader.txt` | `ceca73c504e39084b0b80d2a437aea9988313d1915418d3c629c3e4a8313f5a1` | Loader notice |
| `work/multiplayer/windows/HogsFix-Steam-README.txt` | `82dd6f7e5af13daff2acc172b49502fc548845d0289095323e423f2e063c0c4d` | Steam-specific readme installed by setup |
| `work/multiplayer/mac/original-archive-hashes.txt` | `02a2478f3a9f2ce200b83f69255e57c3da7a4bf2b843bf943f3187e6ef262aa7` | Guard for 140 original MTD/MAD files; contains names and hashes, not game assets |
| `work/dgvoodoo2793/MS/x86/DDraw.dll` | `d45dd48cc4d3ea48bf3b1dda79c11cc75bd5f219d749f8f271e42e3ed6addded` | Unmodified dgVoodoo 2.79.3 x86 DDraw wrapper |
| `work/dgvoodoo2793/MS/x86/D3DImm.dll` | `dcac7eee656c660b081430ed2c6142726fd608e7a335f3dd9651619d42a0408e` | Unmodified dgVoodoo 2.79.3 x86 D3DImm wrapper |
| `work/multiplayer/mac/runtime-payload/dgVoodoo.conf` | `bba407fa7b8280e4845f6c4fd28254978027f8f7dc45b525e2920f61ff223beb` | D3D11 renderer configuration |
| `outputs/HD-профиль Hogs of War/assets/TextureToolkit-x86.asi` | `203f27318705c0ef264e9835102bb329982dd1322807817a436bc42f49be8e53` | Texture injection plugin |
| `outputs/HD-профиль Hogs of War/assets/LICENSE-TextureToolkit.txt` | `f1d24939059eb10722fdcc8e7ed865d2c9326f0255b4db20d60c176346828f6f` | Plugin MIT license |
| `work/multiplayer/windows/DgVoodoo-Redistribution.txt` | `4f36f31a2225f4622422b961873bc2fd81ed41594c2b5a65c8f2ac5353f95977` | dgVoodoo redistribution notice |
| `outputs/HD-профиль Hogs of War/profile-manifest.json` | `b80893576bda229e590917a3f7e05520208d344ef834a841d314a8ec584f1ec3` | 24-page profile metadata |

The 24 DDS files live at `outputs/HD-профиль Hogs of War/payload/TT/inject/` and are SHA-256-pinned individually in `profile-manifest.json`. The build also needs that manifest and all referenced payload files. Do not add `warhogs_.exe`, game archives, Steam data, save files, Wine prefixes, logs, or build outputs to the source repository.

## Rebuild and checks

```sh
work/multiplayer/windows/build.sh
work/multiplayer/windows/verify_static.py
work/multiplayer/windows/build-profile-selftest.sh
```

The headless transaction fixture is a Windows console executable under `work/multiplayer/windows/build/`. Its passing run reported 12 checks for preflight refusal, install, idempotence, receipt-path rejection, restore, and reinstall. It uses a synthetic archive set and does not launch the game. A real Windows PC and a full two-computer game have not been tested.
