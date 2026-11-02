#!/bin/bash
# Flash monster-cyberspace to a connected board, from a release's factory
# image (monster-cyberspace-<board>-vX.Y.Z.bin: bootloader + partitions +
# boot_app0 + app, written at 0x0).
#
# Settings survive: they live past the app slots, which the image never
# reaches. Boards installed before OTA updates arrived kept them at 0x9000;
# this script leaves 0x9000-0xDFFF alone, so the new firmware finds them
# there and moves them on its first boot.
#
# The -ota.bin files are for over-the-air updates, not for this script.
#
# Usage: ./flash.sh [--erase|-E] [monster-cyberspace-<board>-vX.Y.Z.bin] [port]
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
    FIRMWARE=$(ls monster-cyberspace-*.bin 2>/dev/null | grep -v -- '-ota\.bin$' | sort -V | tail -1)
fi

if [[ -z "$FIRMWARE" || ! -f "$FIRMWARE" ]]; then
    echo "Usage: ./flash.sh [--erase|-E] <monster-cyberspace-<board>-vX.Y.Z.bin> [port]"
    exit 1
fi
if [[ "$FIRMWARE" == *-ota.bin ]]; then
    echo "$FIRMWARE is an over-the-air image; flash the factory .bin instead." >&2
    exit 1
fi

if command -v esptool.py >/dev/null 2>&1; then ESPTOOL=(esptool.py)
elif command -v esptool >/dev/null 2>&1; then ESPTOOL=(esptool)
else ESPTOOL=(python3 -m esptool); fi

WRITE=( 0x0 "$FIRMWARE" )
if [[ "$ERASE" == "1" ]]; then
    echo "Erasing $PORT (settings will be lost)..."
    "${ESPTOOL[@]}" --chip auto --port "$PORT" --baud 921600 \
        --before default_reset --after no_reset erase_flash
else
    # Everything but 0x9000-0xDFFF, where older installs kept the settings.
    TMP=$(mktemp -d)
    trap 'rm -rf "$TMP"' EXIT
    head -c $((0x9000)) "$FIRMWARE" > "$TMP/boot.bin"
    tail -c +$((0xE000 + 1)) "$FIRMWARE" > "$TMP/rest.bin"
    WRITE=( 0x0 "$TMP/boot.bin" 0xe000 "$TMP/rest.bin" )
fi

echo "Flashing $FIRMWARE to $PORT..."
"${ESPTOOL[@]}" --chip auto --port "$PORT" --baud 921600 \
    --before default_reset --after hard_reset \
    write_flash -z "${WRITE[@]}"
