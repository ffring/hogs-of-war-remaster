#!/bin/sh
set -eu

HERE="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$HERE/build"
SDK="$(xcrun --sdk macosx --show-sdk-path)"
OUT="$HERE/build/HogsMouseSettingsPanel"
APP="$HERE/build/HogsMouseSettingsPanel.app"

clang -std=c11 -fobjc-arc -Wall -Wextra -Werror \
  -isysroot "$SDK" -mmacosx-version-min=12.0 \
  -I "$HERE/source" \
  "$HERE/source/settings_panel.m" \
  "$HERE/source/settings_store.m" \
  "$HERE/tests/settings_selftest.m" \
  -framework AppKit -framework Foundation \
  -o "$OUT"

mkdir -p "$APP/Contents/MacOS"
cp "$OUT" "$APP/Contents/MacOS/HogsMouseSettingsPanel"
cp "$HERE/source/Info.plist" "$APP/Contents/Info.plist"
codesign --force --deep --sign - "$APP"
codesign --verify --deep --strict "$APP"

echo "Built $OUT"
shasum -a 256 "$OUT"
echo "Built $APP"
shasum -a 256 "$APP/Contents/MacOS/HogsMouseSettingsPanel"
