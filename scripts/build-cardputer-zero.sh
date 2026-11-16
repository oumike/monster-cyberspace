#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Builds the Linux targets (docs/superpowers/specs/2026-11-16-cyberspace-cardputer-zero-design.md §9):
#   scripts/build-cardputer-zero.sh          the CardputerZero (aarch64) in linux/docker's container
#   scripts/build-cardputer-zero.sh --sim    the SDL2 desktop simulator, on this machine
# Options:
#   --fake    the demo build (RC_FAKE_API: scripted, no network; never release)
#   --clean   start from an empty build directory
# Output: linux/build/<cp0|sim>[-fake]/monster-cyberspace
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR/../linux"

TARGET=cp0
FAKE=false
CLEAN=false

usage() { sed -n '3,9s/^# \{0,1\}//p' "$SCRIPT_DIR/$(basename "$0")"; }

for arg in "$@"; do
	case "$arg" in
		--sim) TARGET=sim ;;
		--fake) FAKE=true ;;
		--clean) CLEAN=true ;;
		-h|--help) usage; exit 0 ;;
		*) echo "Unknown option: $arg"; usage; exit 1 ;;
	esac
done

BUILD_DIR="build/$TARGET"
CONFIGURE=(cmake --preset "$TARGET")
if $FAKE; then
	BUILD_DIR="build/$TARGET-fake"
	CONFIGURE+=(-B "$BUILD_DIR" -DMC_FAKE_API=ON)
fi
$CLEAN && rm -rf "$BUILD_DIR"

if [ "$TARGET" = sim ]; then
	nice "${CONFIGURE[@]}"
	nice cmake --build "$BUILD_DIR"
else
	IMAGE=monster-cyberspace-cp0-build
	docker build --quiet --platform linux/arm64 -t "$IMAGE" docker >/dev/null
	docker run --rm --platform linux/arm64 \
		--user "$(id -u):$(id -g)" -e HOME=/tmp \
		-v "$PWD/..:/work" -w /work/linux \
		"$IMAGE" sh -c "nice ${CONFIGURE[*]} && nice cmake --build $BUILD_DIR"
fi

echo "Built linux/$BUILD_DIR/monster-cyberspace"
