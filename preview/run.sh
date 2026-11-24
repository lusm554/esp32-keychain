#!/bin/sh
set -eu

preview_dir=$(CDPATH= cd "$(dirname "$0")" && pwd)
spi_delay=false
if [ "${1:-}" = "--spi-delay" ]; then
  spi_delay=true
  shift
fi

sketch=${1:-"$preview_dir/../firmware/Clock/Clock.ino"}
if [ "$#" -gt 1 ] || [ ! -f "$sketch" ]; then
  echo "Usage: sh preview/run.sh [--spi-delay] [path/to/sketch.ino]" >&2
  exit 1
fi

# Use an absolute path so filenames starting with a dash are passed as files.
case "$sketch" in
  /*) ;;
  *) sketch="$PWD/$sketch" ;;
esac

build_dir=$(mktemp -d "${TMPDIR:-/tmp}/esp32-preview.XXXXXX")
trap 'rm -rf "$build_dir"' EXIT
app="$build_dir/ESP32 Preview.app"
mkdir -p "$app/Contents/MacOS"

cat > "$app/Contents/Info.plist" <<'PLIST'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "https://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleExecutable</key><string>ESP32Preview</string>
  <key>CFBundleIdentifier</key><string>local.esp32-keychain.preview</string>
  <key>CFBundleName</key><string>ESP32 Preview</string>
  <key>CFBundlePackageType</key><string>APPL</string>
</dict>
</plist>
PLIST

xcrun clang++ -std=c++17 -DESP32_PREVIEW -I "$preview_dir" \
  -x c++ -c "$sketch" -o "$build_dir/sketch.o"
xcrun clang++ -std=c++17 -fobjc-arc -framework Cocoa \
  "$preview_dir/ClockPreview.mm" "$build_dir/sketch.o" -o "$app/Contents/MacOS/ESP32Preview"
if [ "$spi_delay" = true ]; then
  open -n -W "$app" --args --spi-delay
else
  open -n -W "$app"
fi
