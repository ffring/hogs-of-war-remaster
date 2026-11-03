#!/bin/zsh
# Fetch and apply Winetricks' pinned DirectPlay recipe to this app's isolated prefix.
# The parent app holds operation.lock for the entire call.
set -eu
umask 077

mode="${1:-}"
[[ "$mode" == install ]] || { print -u2 'Invalid mode.'; exit 2; }
[[ -n "${HOGS_APP_BUNDLE:-}" ]] || { print -u2 'App path missing.'; exit 2; }
APP="${HOGS_APP_BUNDLE:A}"
STATE="$HOME/Library/Application Support/Hogs of War Online"
PREFIX="$STATE/prefix"
WINE="$APP/Contents/SharedSupport/wine/bin/wine"
SERVER="$APP/Contents/SharedSupport/wine/bin/wineserver"
export WINEPREFIX="$PREFIX"
export DYLD_FALLBACK_LIBRARY_PATH="$APP/Contents/Frameworks:/usr/lib"
export WINEDEBUG='-all'
unset WINEARCH WINESERVER WINELOADER WINEDLLPATH
ARCHIVE_URL="${HOGS_DIRECTPLAY_ARCHIVE_URL:-https://web.archive.org/web/20100205000000id_/https://download.microsoft.com/download/E/E/1/EE17FF74-6C45-4575-9CF4-7FC2597ACD18/directx_feb2010_redist.exe}"
ARCHIVE_SHA='f6d191e89a963d7cca34f169d30f49eab99c1ed3bb92da73ec43617caaa1e93f'
CAB_SHA='8254beb9724139e01503ec65e7682da593cc4e81ec4bff431e5fd37ce5656034'

typeset -A EXPECTED
EXPECTED=(
  dplaysvr.exe e3ee1e2d903237131ba42c770f07e7eceaa3574b4ce718062a4e3c1f9cdbf893
  dplayx.dll f50362966d69446f3f562946e77cb72ba8058abcddfadac4d2b0841e07434c3c
  dpmodemx.dll 200753c96fd4ceac9b9bbed12a4b74d5c25df522cb32820dfc58d3d193be8eec
  dpnet.dll b3f41e537797abc414fda5136a864865c37e7324b55a9a5705c6761ff9a8e816
  dpnhpast.dll ab9743ce54837735f0e1cb8840ac58d578093078bc80e0dd91219dc37e5a703d
  dpnhupnp.dll 59e74dd3de40e22aac95133cf746be32cb0de40e4fc801cf63be13d9e2ca8134
  dpnsvr.exe 1ff263d2c1a97d422f94dd3d0872b1fa49de99b825ba64a5bf06f483a1849ac7
  dpwsockx.dll 0725598871b2f1f65ccd0235683873346c8024a38dead045bce20526af65654d
)

die() { print -u2 -- "$1"; exit 1; }
hash_file() { /usr/bin/shasum -a 256 "$1" | /usr/bin/awk '{print $1}'; }
[[ -x "$WINE" && -x "$SERVER" && -d "$PREFIX/drive_c/windows/syswow64" ]] || die 'Игра ещё не подготовлена. Сначала нажми «Играть» и дождись завершения подготовки.'
/usr/bin/pgrep -if '[w]arhogs_?\.exe' >/dev/null && die 'Сначала закрой игру.'

TMP="$STATE/.directplay-install-$$"
BACKUP="$STATE/DirectPlay-backup-$(/bin/date +%Y%m%d-%H%M%S)-$$"
DEST="$PREFIX/drive_c/windows/syswow64"
/bin/mkdir -m 700 "$TMP" "$BACKUP"
typeset -a names
names=(dplaysvr.exe dplayx.dll dpmodemx.dll dpnet.dll dpnhpast.dll dpnhupnp.dll dpnsvr.exe dpwsockx.dll)
typeset -A previous_kind
committed=0
files_mutated=0

rollback() {
  (( committed == 1 || files_mutated == 0 )) && return
  "$SERVER" -k >/dev/null 2>&1 || true
  for name in $names; do
    target="$DEST/$name"
    /bin/rm -f "$target"
    case "${previous_kind[$name]:-absent}" in
      file) /usr/bin/ditto "$BACKUP/files/$name" "$target" ;;
      symlink) /bin/ln -s "$(<"$BACKUP/links/$name")" "$target" ;;
    esac
  done
  if [[ -f "$BACKUP/system.reg" ]]; then
    /usr/bin/ditto "$BACKUP/system.reg" "$PREFIX/system.reg"
  else
    /bin/rm -f "$PREFIX/system.reg"
  fi
  /bin/rm -f "$PREFIX/.hogs-directplay-v1"
  if [[ -f "$BACKUP/ready-marker" ]]; then
    /usr/bin/ditto "$BACKUP/ready-marker" "$PREFIX/.hogs-directplay-v1"
  fi
}
cleanup() {
  task_rc=$?
  trap - EXIT HUP INT TERM
  if (( task_rc != 0 )); then rollback; fi
  /bin/rm -rf "$TMP"
  exit $task_rc
}
trap cleanup EXIT
trap 'exit 129' HUP
trap 'exit 130' INT
trap 'exit 143' TERM

/bin/mkdir "$TMP/files" "$BACKUP/files" "$BACKUP/links"
for name in $names; do
  target="$DEST/$name"
  if [[ -L "$target" ]]; then
    previous_kind[$name]=symlink
    /usr/bin/readlink "$target" > "$BACKUP/links/$name"
  elif [[ -f "$target" ]]; then
    previous_kind[$name]=file
    /usr/bin/ditto "$target" "$BACKUP/files/$name"
  else
    previous_kind[$name]=absent
  fi
done
[[ -f "$PREFIX/system.reg" ]] && /usr/bin/ditto "$PREFIX/system.reg" "$BACKUP/system.reg"
[[ -f "$PREFIX/.hogs-directplay-v1" ]] && /usr/bin/ditto "$PREFIX/.hogs-directplay-v1" "$BACKUP/ready-marker"

archive="$TMP/directx_feb2010_redist.exe"
cab="$TMP/dxnt.cab"
print 'Скачивается исходный пакет компонентов…'
/usr/bin/curl --fail --location --retry 2 --connect-timeout 20 --max-time 900 "$ARCHIVE_URL" -o "$archive"
[[ "$(hash_file "$archive")" == "$ARCHIVE_SHA" ]] || die 'Проверка исходного пакета не прошла. Файлы Wine не изменены.'
/usr/bin/bsdtar -xOf "$archive" dxnt.cab > "$cab"
[[ "$(hash_file "$cab")" == "$CAB_SHA" ]] || die 'Проверка архива DirectPlay не прошла. Файлы Wine не изменены.'
for name in $names; do
  /usr/bin/bsdtar -xOf "$cab" "$name" > "$TMP/files/$name"
  [[ "$(hash_file "$TMP/files/$name")" == "${EXPECTED[$name]}" ]] || die "Проверка файла $name не прошла. Файлы Wine не изменены."
done

files_mutated=1
for name in $names; do /usr/bin/ditto "$TMP/files/$name" "$DEST/$name"; done
export WINEDLLOVERRIDES='dplaysvr.exe,dplayx,dpmodemx,dpnet,dpnhpast,dpnhupnp,dpnsvr.exe,dpwsockx=n'
for name in dplayx.dll dpnet.dll dpnhpast.dll dpnhupnp.dll; do
  "$WINE" 'C:\windows\syswow64\regsvr32.exe' /S "$name" || die "Не удалось подготовить компонент $name. Изменения будут отменены."
done
"$SERVER" -w || die 'Wine не завершил регистрацию компонентов. Изменения будут отменены.'
for name in $names; do
  [[ "$(hash_file "$DEST/$name")" == "${EXPECTED[$name]}" ]] || die "Проверка установленного файла $name не прошла. Изменения будут отменены."
done
print -- "DirectPlay source SHA-256: $ARCHIVE_SHA" > "$TMP/marker"
print -- "dxnt.cab SHA-256: $CAB_SHA" >> "$TMP/marker"
for name in $names; do print -- "$name ${EXPECTED[$name]}" >> "$TMP/marker"; done
/bin/mv "$TMP/marker" "$PREFIX/.hogs-directplay-v1"
committed=1
print 'Сетевой режим подготовлен.'
