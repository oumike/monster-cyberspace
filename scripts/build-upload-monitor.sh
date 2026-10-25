#!/usr/bin/env bash
# Build, flash and open the serial monitor. Usage: scripts/build-upload-monitor.sh [env]
# env defaults to m9; m9_fake runs the scripted fake API (no account needed).
set -euo pipefail
cd "$(dirname "$0")/.."
ENV="${1:-m9}"
pio run -e "$ENV" -t upload
pio device monitor -b 115200
