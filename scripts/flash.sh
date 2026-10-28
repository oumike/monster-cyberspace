#!/bin/bash
# Flash monster-cyberspace to a connected board, from a release's images.
#
# The factory image (monster-cyberspace-<board>-vX.Y.Z.bin: bootloader +
# partitions + boot_app0 + app) is written at 0x0. Its blank settings area
# wipes the saved settings (Wi-Fi, sign-in), so use it for a first install.
# The app-only image (…-app.bin) is written at 0x10000 and keeps the settings,
# so use it to update a board that already runs Monster Cyberspace.
#
# Usage: ./flash.sh [--erase|-E] <monster-cyberspace-<board>-vX.Y.Z[-app].bin> [port]
#   --erase, -E   Erase the whole chip first (wipes the settings).
# Default port: /dev/ttyUSB0 (Linux); on macOS use /dev/cu.usbmodem* or
# /dev/cu.wchusbserial*.

set -e

ERASE=0
ARGS=()
for arg in "$@"; do
    case "$arg" in
        --erase|-E) ERASE=1 ;;
        *)          ARGS+=("$arg") ;;
    esac
done

FIRMWARE=${ARGS[0]:-}
PORT=${ARGS[1]:-/dev/ttyUSB0}

if [[ -z "$FIRMWARE" ]]; then
    # Fall back to the only factory image in the current directory.
    FIRMWARE=$(ls monster-cyberspace-*.bin 2>/dev/null | grep -v -- '-app\.bin$' | sort -V | tail -1)
fi

if [[ -z "$FIRMWARE" || ! -f "$FIRMWARE" ]]; then
    echo "Usage: ./flash.sh [--erase|-E] <monster-cyberspace-<board>-vX.Y.Z[-app].bin> [port]"
    exit 1
fi

ADDR=0x0
if [[ "$FIRMWARE" == *-app.bin ]]; then
    ADDR=0x10000
    if [[ "$ERASE" == "1" ]]; then
        echo "--erase with an app-only image would leave no bootloader; use the factory .bin." >&2
        exit 1
    fi
fi

if command -v esptool.py >/dev/null 2>&1; then ESPTOOL=(esptool.py)
elif command -v esptool >/dev/null 2>&1; then ESPTOOL=(esptool)
else ESPTOOL=(python3 -m esptool); fi

if [[ "$ERASE" == "1" ]]; then
    echo "Erasing $PORT (settings will be lost)..."
    "${ESPTOOL[@]}" --chip esp32s3 --port "$PORT" --baud 921600 \
        --before default_reset --after no_reset erase_flash
fi

echo "Flashing $FIRMWARE to $PORT at $ADDR..."
"${ESPTOOL[@]}" --chip esp32s3 --port "$PORT" --baud 921600 \
    --before default_reset --after hard_reset \
    write_flash -z "$ADDR" "$FIRMWARE"
