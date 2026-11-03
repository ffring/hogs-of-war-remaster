#!/usr/bin/env python3
"""Build a separate Mac test installer. Never writes the installed game or prefix."""
from pathlib import Path
import hashlib,plistlib,shutil,subprocess,json,os

ROOT=Path(__file__).resolve().parents[3]
HERE=Path(__file__).resolve().parent
BASE=Path(os.environ.get('HOGS_WINE_APP','/Applications/Hogs of War Online.app'))
SEED=HERE/'runtime-payload'
HD_PROFILE=ROOT/'outputs/HD-профиль Hogs of War'
HD_PAYLOAD=HD_PROFILE/'payload/TT/inject'
STAGE=HERE/'package-root'
APP=STAGE/'Applications/Hogs of War Online.app'
C=APP/'Contents'
OUT=ROOT/'outputs/Hogs-of-War-Mac-Setup.pkg'

def run(*args):subprocess.run([str(a) for a in args],check=True)

if APP.exists():shutil.rmtree(APP)
APP.parent.mkdir(parents=True,exist_ok=True)
# APFS cloning avoids a second physical runtime copy while the owner plays.
run('/bin/cp','-cR',BASE,APP)
for name in ['MacOS','Resources','_CodeSignature']:
    p=C/name
    if p.is_dir():shutil.rmtree(p)
    p.mkdir()
# Unused alternative renderers and wrapper SDK are absent from this specific port.
for name in ['renderer/d3dmetal','renderer/apple_gptk','renderer/dxmt','renderer/d9vk','renderer/cnc_ddraw','SikarugirSdk.framework']:
    p=C/'Frameworks'/name
    if p.is_symlink():p.unlink()
    elif p.exists():shutil.rmtree(p)
# Wine's wineserver and winebus load libinotify through @rpath. Resolve that
# run path inside this relocatable app rather than relying on the host search path.
wine_lib=C/'SharedSupport/wine/lib'
inotify=wine_lib/'libinotify.0.dylib'
if inotify.exists() or inotify.is_symlink():inotify.unlink()
inotify.symlink_to('../../../Frameworks/libinotify.0.dylib')
R=C/'Resources'
shutil.copy2(BASE/'Contents/Resources/Hogs.icns',R/'Hogs.icns')
run('clang','-arch','arm64','-arch','x86_64','-fobjc-arc','-fblocks','-mmacosx-version-min=14.6','-O2','-Wall','-Wextra','-Wno-unused-parameter','-Wno-deprecated-declarations',HERE/'hogs-online.m','-framework','Cocoa','-o',C/'MacOS/hogs-online')
for helper in ['download-steam.command','prepare-directplay.command']:
    shutil.copy2(HERE/helper,R/helper);(R/helper).chmod(0o755)
payload=R/'payload';payload.mkdir()
names=['HogsFix.asi','HogsFix.ini','dinput.dll','ddraw.dll','d3dimm.dll','d3d11.dll','dxgi.dll','d3d10core.dll','dgVoodoo.conf','dxvk.conf']
manifest={}
for name in names:
    shutil.copy2(SEED/name,payload/name)
    manifest[name]=hashlib.sha256((payload/name).read_bytes()).hexdigest()
toolkit=HD_PROFILE/'assets/TextureToolkit-x86.asi'
shutil.copy2(toolkit,payload/'TextureToolkit-x86.asi')
manifest['TextureToolkit-x86.asi']=hashlib.sha256((payload/'TextureToolkit-x86.asi').read_bytes()).hexdigest()
ini='''[TextureToolkit]\nResourceRoot=TT\nEnableInjection=1\nFinalRGBAReadback=1\nAutoDump=0\nVerbose=0\nFilterSmallTextures=0\nHighlightSelected=0\n'''
(payload/'TextureToolkit.ini').write_text(ini)
manifest['TextureToolkit.ini']=hashlib.sha256((payload/'TextureToolkit.ini').read_bytes()).hexdigest()
page_names=sorted(p.name for p in HD_PAYLOAD.glob('*.dds'))
if len(page_names)!=24:
    raise RuntimeError(f'Expected 24 pinned HD pages, found {len(page_names)}')
for name in page_names:
    source=HD_PAYLOAD/name
    target=payload/'TT/inject'/name
    target.parent.mkdir(parents=True,exist_ok=True)
    shutil.copy2(source,target)
    manifest['TT/inject/'+name]=hashlib.sha256(target.read_bytes()).hexdigest()
if manifest['TextureToolkit-x86.asi']!='203f27318705c0ef264e9835102bb329982dd1322807817a436bc42f49be8e53':
    raise RuntimeError('TextureToolkit payload is not the approved 203f build')
if len(manifest)!=36:
    raise RuntimeError(f'Unexpected runtime payload count: {len(manifest)}')
(R/'payload.plist').write_bytes(plistlib.dumps(manifest))
shutil.copy2(HERE/'original-archive-hashes.txt',R/'original-archive-hashes.txt')
manifest_hash=hashlib.sha256((R/'original-archive-hashes.txt').read_bytes()).hexdigest()
shutil.copytree(HERE/'third-party',R/'Third Party Notices')
(R/'Third Party Notices/TextureToolkit-LICENSE.txt').write_bytes((HD_PROFILE/'assets/LICENSE-TextureToolkit.txt').read_bytes())
(R/'Third Party Notices/TextureToolkit-source-adaptation.patch').write_bytes((HD_PROFILE/'assets/combined-final-rgba-mutable.patch').read_bytes())
(R/'Third Party Notices/Sources.txt').write_text('''This package includes 24 HD texture pages and the Texture Toolkit component. It does not include complete Hogs of War game archives, the game executable, Steam credentials, or saves. The game archives and models are copied from the user's own Steam installation and remain unchanged. The texture pages replace selected graphics at run time; they do not enlarge the game's archive files. Coverage is limited to the captured characters and environments.
Wine 10.0 revision3 / Sikarugir engines: https://github.com/Sikarugir-App/Engines/releases/tag/v1.0
Wine corresponding source: https://github.com/Gcenx/winecx
Wine upstream: https://gitlab.winehq.org/wine/wine
DXVK-Sikarugir1.10.3-async: https://github.com/Sikarugir-App/DirectX
DXVK upstream: https://github.com/doitsujin/dxvk
dgVoodoo2.79.3 unmodified DDraw and D3DImm components, for this game-specific mod only:
https://dege.freeweb.hu/dgVoodoo2/ and https://dege.freeweb.hu/dgVoodoo2/ReadmeGeneral/
HogsFix0.1: https://github.com/Torisutan/HogsFix
Ultimate ASI Loader: https://github.com/ThirteenAG/Ultimate-ASI-Loader
Texture Toolkit (MIT) and the local source adaptation patch are included in this folder. The source project is https://github.com/BadassBaboon/Texture-Toolkit .
The native launcher and Steam download helper sources accompany the installer.
The DirectPlay preparation helper downloads and verifies Microsoft's original February 2010 redistributable only after the user selects network preparation. It does not bundle Microsoft DirectPlay binaries. The Russian DirectX EULA text is included in Third Party Notices/Microsoft-DirectX-EULA-ru.txt; it was extracted from the pinned redistributable's DSETUP32.dll resource.
Unused D3DMetal/Apple GPTK components are not included.
''')
guide='''Проверка игры по сети

1. На обоих компьютерах подготовьте Hogs of War из своих копий Steam.
2. Установите Tailscale на оба компьютера и подключите их к одной приватной сети. Каждый игрок входит в свою учётную запись Tailscale.
3. На Mac нажмите «Подготовить сетевой режим», дождитесь завершения, затем нажмите «Играть по сети».
4. В игре на Mac выберите Multiplayer → Network → TCP/IP. Mac создаёт лобби и оставляет поле адреса пустым.
5. На Windows выберите Multiplayer → TCP/IP и введите адрес Mac из Tailscale (обычно 100.x.x.x).

На первом запуске Mac выберите dgVoodoo DirectX Wrapper, 640×480, затем Start.
Для обычной игры используйте кнопку «Играть».

Проверен обмен DirectPlay между двумя процессами на одном Mac. Матч между разными компьютерами и работа через Tailscale ещё не проверены. Если подключение не сработает, не отключайте брандмауэр — передайте журналы запуска для проверки.

Передавать другу пароль Steam или Tailscale не нужно. Добавьте его в приватную сеть через приложение Tailscale.

При подготовке проверяются игровые архивы Steam. Установщик создаёт отдельную копию игры и сохраняет прежнюю папку рядом с ней. HD-профиль содержит 24 страницы: часть формы и лиц, а также окружение. Полного HD-покрытия игры пока нет.
'''
(R/'Как играть по сети.txt').write_text(guide)
info={'CFBundleIdentifier':'local.hogsofwar.online','CFBundleName':'Hogs of War Online','CFBundleDisplayName':'Hogs of War Online','CFBundleExecutable':'hogs-online','CFBundleIconFile':'Hogs.icns','CFBundlePackageType':'APPL','CFBundleVersion':'0.5.0','CFBundleShortVersionString':'0.5','LSMinimumSystemVersion':'14.6','NSHighResolutionCapable':False,'NSPrincipalClass':'NSApplication'}
(C/'Info.plist').write_bytes(plistlib.dumps(info))
run('/usr/bin/codesign','--force','--deep','--sign','-',APP)
run('/usr/bin/codesign','--verify','--deep','--strict',APP)
run(C/'MacOS/hogs-online','--check') # File/hash-only check; no NSApplication or Wine.
scripts=HERE/'pkg-scripts';scripts.mkdir(exist_ok=True)
(scripts/'preinstall').write_text('''#!/bin/sh
if /usr/bin/pgrep -if '[w]arhogs_?\\.exe' >/dev/null; then
  echo 'Сначала выйдите из Hogs of War, затем повторите установку.' >&2
  exit 1
fi
exit 0
''');(scripts/'preinstall').chmod(0o755)
OUT.parent.mkdir(exist_ok=True)
components=HERE/'components.plist'
run('/usr/bin/pkgbuild','--analyze','--root',STAGE,components)
component_data=plistlib.loads(components.read_bytes())
def disable_relocation(component):
    component['BundleIsRelocatable']=False
    for child in component.get('ChildBundles',[]):
        disable_relocation(child)
for component in component_data:
    disable_relocation(component)
components.write_bytes(plistlib.dumps(component_data))
run('/usr/bin/nice','-n','15','/usr/bin/pkgbuild','--root',STAGE,'--component-plist',components,'--identifier','local.hogsofwar.online.installer','--version','0.5.0','--install-location','/','--scripts',scripts,OUT)
record={'version':'0.5.0','status':'network_test_package_game_acceptance_pending','source_archives_bundled':False,'game_executables_bundled':False,'steam_credentials_bundled':False,'saves_bundled':False,'hd_profile_pages_included':24,'texture_toolkit_sha256':manifest['TextureToolkit-x86.asi'],'old_resized_archive_patch_included':False,'mouse_component_included':False,'known_original_archive_hash_guard_sha256':manifest_hash,'safe_source_folder':'HogsofWar; a pre-existing folder is preserved as a sibling backup before replacement','wine_started_by_builder':False,'directplay_setup':'on-demand user action; original redistributable downloaded and SHA-256 verified; no Microsoft binaries bundled','directplay_redist_sha256':'f6d191e89a963d7cca34f169d30f49eab99c1ed3bb92da73ec43617caaa1e93f','directplay_eula_sha256':hashlib.sha256((HERE/'third-party/Microsoft-DirectX-EULA-ru.txt').read_bytes()).hexdigest(),'directplay_helper_positive_test':'completed four upstream x86 registrations in a newly initialized isolated prefix; all eight payload hashes and ready marker verified','directplay_helper_negative_test':'forced curl failure; all eight existing DLL sentinels, system.reg, and ready marker remained byte-for-byte unchanged','payload_sha256':manifest,'installer_sha256':hashlib.sha256(OUT.read_bytes()).hexdigest(),'installer_bytes':OUT.stat().st_size,'min_macos':'14.6','architectures':['arm64','x86_64'],'live_app_modified':False,'live_prefix_modified':False,'signature':'ad-hoc app; unsigned pkg; no Developer ID notarization','game_or_network_match_test':'not performed'}
(HERE/'build-record.json').write_text(json.dumps(record,indent=2)+'\n')
print(json.dumps({k:v for k,v in record.items() if k!='payload_sha256'},indent=2))
