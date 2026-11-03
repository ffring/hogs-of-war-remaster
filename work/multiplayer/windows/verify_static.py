#!/usr/bin/env python3
"""Static packaging checks for the Windows Hogs of War setup."""
from pathlib import Path
import hashlib
import json
import struct

root = Path(__file__).resolve().parents[3]
out = root / "outputs"
exe = out / "Hogs-of-War-Windows-Setup.exe"
assert exe.is_file(), "build the Windows installer first"
data = exe.read_bytes()
assert data[:2] == b"MZ"
pe = struct.unpack_from("<I", data, 0x3C)[0]
assert data[pe:pe+4] == b"PE\0\0"
assert struct.unpack_from("<H", data, pe+4)[0] == 0x8664, "expected x64 Windows GUI"

manifest_path = out / "HD-профиль Hogs of War" / "profile-manifest.json"
profile_root = manifest_path.parent
manifest = json.loads(manifest_path.read_text())
assert manifest["page_count"] == 24 and len(manifest["pages"]) == 24
for page in manifest["pages"]:
    blob = (profile_root / "payload" / "TT" / "inject" / page["filename"]).read_bytes()
    assert hashlib.sha256(blob).hexdigest() == page["sha256"]
    assert blob in data, f"missing embedded page {page['filename']}"

for rel, expected in [
    ("work/HogsFix/dinput.dll", "d5a059aa467a7a7127c8f6169f79fa63ff0f55986ee9eb2fd9a281bebf2aa2e6"),
    ("work/HogsFix/HogsFix.asi", "68d73e0803fb62edb739539cf3d78c41e355e2e5030acb7b056c409b49a96e3d"),
    ("work/dgvoodoo2793/MS/x86/DDraw.dll", "d45dd48cc4d3ea48bf3b1dda79c11cc75bd5f219d749f8f271e42e3ed6addded"),
    ("work/dgvoodoo2793/MS/x86/D3DImm.dll", "dcac7eee656c660b081430ed2c6142726fd608e7a335f3dd9651619d42a0408e"),
    ("outputs/HD-профиль Hogs of War/assets/TextureToolkit-x86.asi", "203f27318705c0ef264e9835102bb329982dd1322807817a436bc42f49be8e53"),
    ("outputs/HD-профиль Hogs of War/assets/LICENSE-TextureToolkit.txt", "f1d24939059eb10722fdcc8e7ed865d2c9326f0255b4db20d60c176346828f6f"),
    ("work/HogsFix/LICENSE-UltimateASILoader.txt", None),
    ("work/multiplayer/mac/runtime-payload/dgVoodoo.conf", None),
    ("work/multiplayer/windows/DgVoodoo-Redistribution.txt", None),
]:
    src = root / rel
    raw = src.read_bytes()
    if expected is not None:
        assert hashlib.sha256(raw).hexdigest() == expected, f"unexpected source hash: {rel}"
    assert raw in data, f"missing embedded file: {rel}"

# Ensure the installer carries the original-archive guard and none of the withdrawn payloads.
guard = (root / "work/multiplayer/mac/original-archive-hashes.txt").read_bytes()
assert guard.startswith(b"HOGSORG1\n") and guard in data
assert len(guard.splitlines()) == 141, "expected magic plus 140 archive hashes"
assert b"HOGSPAT3" not in data
payload_source = (root / "work/multiplayer/windows/payload.S").read_text()
assert "HogsMouse" not in payload_source and "texture_patch" not in payload_source
assert "warhogs_.exe" not in payload_source, "game executable must not be bundled"
assert "dxvk" not in payload_source.lower() and "moltenvk" not in payload_source.lower()

source = (root / "work/multiplayer/windows/installer.c").read_text()
build = (root / "work/multiplayer/windows/build.sh").read_text()
assert "hogs_gpu_profile_install" in source and "hogs_gpu_profile_restore" in source
assert "hogs_archive_guard_check" in source
assert "mouse_component" not in source and "texture_patch" not in source
assert "texture_patch_win.c" not in build and "mouse_component_win.c" not in build

print(f"PASS: x64 setup, 24 HD pages, licensed toolkit/dgVoodoo payload, original 140-archive guard; {len(data):,} bytes")
