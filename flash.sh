#!/usr/bin/env bash
# Flash Bee: compile + flash to a XIAO ESP32-C3 with arduino-cli.
# Usage: bash ~/Developer/"Flash Bee"/flash.sh [c3|c6]
set -euo pipefail

BOARD="${1:-c3}"
case "$BOARD" in
  c3) FQBN="esp32:esp32:XIAO_ESP32C3" ;;
  c6) FQBN="esp32:esp32:XIAO_ESP32C6" ;;
  *) echo "Usage: $0 [c3|c6]"; exit 1 ;;
esac

ESP32_URL="https://espressif.github.io/arduino-esp32/package_esp32_index.json"
SKETCH="$(cd "$(dirname "$0")/FlashBee" && pwd)"
export ARDUINO_LIBRARY_ENABLE_UNSAFE_INSTALL=true   # allows lib install from git URLs

command -v arduino-cli >/dev/null || brew install arduino-cli

echo "== ESP32 core"
arduino-cli core update-index --additional-urls "$ESP32_URL"
arduino-cli core install esp32:esp32 --additional-urls "$ESP32_URL"

echo "== Libraries"
LIBDIR="$(arduino-cli config get directories.user 2>/dev/null || echo "$HOME/Documents/Arduino")/libraries"
[ -d "$LIBDIR/Seeed_GFX" ] || [ -d "$LIBDIR/Seeed_GFX-master" ] || \
  arduino-cli lib install --git-url https://github.com/Seeed-Studio/Seeed_GFX.git
[ -d "$LIBDIR/Seeed_Arduino_RoundDisplay" ] || [ -d "$LIBDIR/Seeed_Arduino_Round_display" ] || \
  arduino-cli lib install --git-url https://github.com/Seeed-Studio/Seeed_Arduino_RoundDisplay.git
[ -d "$LIBDIR/PubSubClient" ] || arduino-cli lib install PubSubClient
if [ -d "$LIBDIR/TFT_eSPI" ]; then
  echo "WARNING: $LIBDIR/TFT_eSPI conflicts with Seeed_GFX. Rename or remove it if the build picks the wrong one."
fi

echo "== Compile ($FQBN)"
arduino-cli compile --fqbn "$FQBN" "$SKETCH"

echo "== Find port"
PORT="$(ls /dev/cu.usbmodem* 2>/dev/null | head -n1 || true)"
if [ -z "$PORT" ]; then
  echo "No /dev/cu.usbmodem* port. Plug in the XIAO with a data cable."
  echo "If it still doesn't show: hold BOOT, tap RESET, release BOOT, then rerun."
  exit 1
fi
echo "Port: $PORT"

echo "== Upload"
arduino-cli upload -p "$PORT" --fqbn "$FQBN" "$SKETCH"

echo "== Serial monitor (Ctrl-C to exit)"
sleep 2
PORT="$(ls /dev/cu.usbmodem* 2>/dev/null | head -n1)"
arduino-cli monitor -p "$PORT" -c baudrate=115200
