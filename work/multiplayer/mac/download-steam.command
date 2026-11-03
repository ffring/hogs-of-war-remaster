#!/bin/zsh
set -eu
umask 077
app_path="${0:A:h:h:h}"
state_dir="$HOME/Library/Application Support/Hogs of War Online"
game_dir="$state_dir/Steam-download"
tool_dir="$state_dir/SteamCMD/MacOS"
/bin/mkdir -p "$state_dir"
: >> "$state_dir/operation.lock"
zmodload zsh/system
if ! zsystem flock -t 0 -f download_lock_fd "$state_dir/operation.lock"; then
  print 'Подготовка уже идёт в другом окне. Дождись её завершения.'
  read -r '?Нажми Enter, чтобы закрыть окно.'
  exit 1
fi
if /usr/bin/pgrep -if '[w]arhogs_?\.exe' >/dev/null; then
  print 'Сначала выйди из Hogs of War, затем повтори скачивание.'
  read -r '?Нажми Enter, чтобы закрыть окно.'
  exit 1
fi
if ! /usr/bin/arch -x86_64 /usr/bin/true 2>/dev/null; then
  print 'Для игры требуется Rosetta. Сейчас откроется установка Apple.'
  /usr/sbin/softwareupdate --install-rosetta
fi
/bin/mkdir -p "$tool_dir" "$game_dir"
if [[ ! -x "$tool_dir/steamcmd.sh" ]]; then
  print 'Скачиваем клиент Steam…'
  /usr/bin/curl --fail --location --retry 2 'https://steamcdn-a.akamaihd.net/client/installer/steamcmd_osx.tar.gz' -o "$state_dir/steamcmd_osx.tar.gz"
  /usr/bin/tar -xzf "$state_dir/steamcmd_osx.tar.gz" -C "$tool_dir"
fi
print '\nВойди в Steam, где у тебя куплена Hogs of War.'
read -r 'steam_user?Логин Steam: '
if [[ -z "$steam_user" ]]; then exit 1; fi
print 'Пароль и Steam Guard вводятся в Steam ниже. В чат их отправлять не нужно.\n'
if ! "$tool_dir/steamcmd.sh" +@sSteamCmdForcePlatformType windows +force_install_dir "$game_dir" +login "$steam_user" +app_update 389900 validate +quit; then
  print 'Steam не завершил скачивание. Проверь сообщение выше и повтори попытку.'
  read -r '?Нажми Enter, чтобы закрыть окно.'
  exit 1
fi
if [[ ! -f "$game_dir/warhogs_.exe" ]]; then
  print 'Steam не завершил скачивание игры. Проверь сообщение выше и повтори попытку.'
  read -r '?Нажми Enter, чтобы закрыть окно.'
  exit 1
fi
print '\nСкачивание завершено. Продолжаем подготовку в Hogs of War Online.'
zsystem flock -u "$download_lock_fd"
/usr/bin/open -a "$app_path" "$game_dir"
read -r '?Нажми Enter, чтобы закрыть окно.'
