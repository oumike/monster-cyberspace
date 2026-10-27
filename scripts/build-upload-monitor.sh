#!/usr/bin/env bash
# Build, flash and open the serial monitor. Run with --help for the options.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR/.."

ENV_NAME=""
BOARD=""
PORT=""
ERASE_FIRST=false
FULLCLEAN=false
JUST_BUILD=false

# Every firmware [env:NAME] in platformio.ini, in file order. Read from the file
# so a new board's environment shows up here the day it lands. `native` holds
# the host unit tests and is not firmware.
firmware_envs() {
	sed -n 's/^\[env:\(.*\)\]$/\1/p' platformio.ini | grep -vx native
}

has_env() {
	firmware_envs | grep -qx "$1"
}

env_label() {
	case "$1" in
		m9)     echo "Elecrow ThinkNode M9" ;;
		wio_l2) echo "Seeed Wio Tracker L2" ;;
		tdeck)  echo "LilyGO T-Deck Plus" ;;
		crowpanel35) echo "Elecrow CrowPanel Advance 3.5\"" ;;
		crowpanel50) echo "Elecrow CrowPanel Advance 5.0\"" ;;
		pager)  echo "LilyGO T-LoRa Pager" ;;
		*)      echo "$1" ;;
	esac
}

select_env() {
	if ! has_env "$1"; then
		echo "Environment '$1' not found in platformio.ini"
		exit 1
	fi
	ENV_NAME="$1"
}

prompt_for_device() {
	local options=()
	while IFS= read -r env; do
		[ -n "$env" ] && options+=("$env")
	done < <(firmware_envs)

	if [ "${#options[@]}" -eq 0 ]; then
		echo "No firmware environments found in platformio.ini"
		exit 1
	fi

	if [ ! -t 0 ]; then
		ENV_NAME="${options[0]}"
		echo "[PIO] Non-interactive shell detected, using env: $ENV_NAME"
		return
	fi

	echo "Select the firmware to build:"
	for i in "${!options[@]}"; do
		printf "  %d) %s (%s)\n" "$((i + 1))" "$(env_label "${options[$i]}")" "${options[$i]}"
	done

	while true; do
		read -r -p "Enter choice [1-${#options[@]}]: " choice
		if [[ "$choice" =~ ^[0-9]+$ ]] && [ "$choice" -ge 1 ] && [ "$choice" -le "${#options[@]}" ]; then
			ENV_NAME="${options[$((choice - 1))]}"
			echo "[PIO] Selected env: $ENV_NAME"
			return
		fi
		echo "Invalid selection. Please choose a number between 1 and ${#options[@]}."
	done
}

show_usage() {
	cat <<USAGE
Usage: $0 [ENV] [--m9|-9] [--wio-l2|-w] [--tdeck|-t] [--crowpanel35|-3] [--crowpanel50|-5] [--pager|-P] [--port|-p PORT] [--erase|-E] [--fullclean|-F] [--just-build|-B]
  ENV               Any firmware environment in platformio.ini (e.g. m9, tdeck).
  Board flags (one per supported board):
  --m9, -9          Elecrow ThinkNode M9 (m9)
  --wio-l2, -w      Seeed Wio Tracker L2 (wio_l2)
  --tdeck, -t       LilyGO T-Deck Plus (tdeck)
  --crowpanel35, -3 Elecrow CrowPanel Advance 3.5" (crowpanel35)
  --crowpanel50, -5 Elecrow CrowPanel Advance 5.0" (crowpanel50)
  --pager, -P       LilyGO T-LoRa Pager (pager)
                    With no board or environment given, you'll be prompted.
  --port, -p PORT   Serial port for upload and monitor (default: auto-detect)
  --erase, -E       Erase the whole flash while uploading (wipes saved settings)
  --fullclean, -F   Run PlatformIO fullclean before building
  --just-build, -B  Compile only: no upload, no monitor, no device needed.
                    Builds every firmware environment, or just the one named.
                    Keeps going after a failure, prints a pass/fail summary and
                    exits non-zero if any environment failed.
USAGE
}

format_duration() {
	local total="$1"
	printf "%dm %02ds" "$((total / 60))" "$((total % 60))"
}

if ! command -v pio >/dev/null 2>&1; then
	echo "PlatformIO CLI not found: install it or run from an environment that provides 'pio'."
	exit 1
fi

while [ "$#" -gt 0 ]; do
	case "$1" in
		--m9|-9)         BOARD=m9 ;;
		--wio-l2|-w)     BOARD=wio_l2 ;;
		--tdeck|-t)      BOARD=tdeck ;;
		--crowpanel35|-3) BOARD=crowpanel35 ;;
		--crowpanel50|-5) BOARD=crowpanel50 ;;
		--pager|-P)      BOARD=pager ;;
		--port|-p)
			[ "$#" -ge 2 ] || { echo "--port needs a value"; exit 1; }
			PORT="$2"
			shift
			;;
		--erase|-E)      ERASE_FIRST=true ;;
		--fullclean|-F)  FULLCLEAN=true ;;
		--just-build|-B) JUST_BUILD=true ;;
		--help|-h)       show_usage; exit 0 ;;
		-*)
			echo "Unknown argument: $1"
			show_usage
			exit 1
			;;
		*)               select_env "$1" ;;
	esac
	shift
done

if [ -n "$BOARD" ]; then
	[ -z "$ENV_NAME" ] || { echo "Give either a board flag or an environment name, not both."; exit 1; }
	select_env "$BOARD"
fi

if [ "$JUST_BUILD" = true ]; then
	if [ "$ERASE_FIRST" = true ]; then
		echo "--erase needs a connected device; it cannot be combined with --just-build."
		exit 1
	fi

	BUILD_ENVS=()
	if [ -n "$ENV_NAME" ]; then
		BUILD_ENVS+=("$ENV_NAME")
	else
		while IFS= read -r env; do
			[ -n "$env" ] && BUILD_ENVS+=("$env")
		done < <(firmware_envs)
	fi

	echo "[PIO] Build-only sweep over ${#BUILD_ENVS[@]} environment(s)."
	SWEEP_START_TS="$(date +%s)"
	RESULTS=()
	FAILED=0

	for env in "${BUILD_ENVS[@]}"; do
		echo
		echo "===================================================================="
		echo "[PIO] Building $env ($(env_label "$env"))"
		echo "===================================================================="
		ENV_START_TS="$(date +%s)"
		if [ "$FULLCLEAN" = true ] && ! pio run -e "$env" -t fullclean; then
			RESULTS+=("FAIL  $env  (fullclean)  $(format_duration $(( $(date +%s) - ENV_START_TS )))")
			FAILED=$((FAILED + 1))
			continue
		fi
		if pio run -e "$env"; then
			SIZE_NOTE=""
			ENV_BIN=".pio/build/${env}/firmware.bin"
			if [ -f "$ENV_BIN" ]; then
				SIZE_NOTE="  $(( $(wc -c <"$ENV_BIN") / 1024 )) KB"
			fi
			RESULTS+=("ok    $env  $(format_duration $(( $(date +%s) - ENV_START_TS )))${SIZE_NOTE}")
		else
			RESULTS+=("FAIL  $env  $(format_duration $(( $(date +%s) - ENV_START_TS )))")
			FAILED=$((FAILED + 1))
		fi
	done

	echo
	echo "===================================================================="
	echo "[PIO] Build summary"
	echo "===================================================================="
	for line in "${RESULTS[@]}"; do
		echo "  $line"
	done
	echo
	echo "[PIO] ${#BUILD_ENVS[@]} environment(s), $FAILED failed, total $(format_duration $(( $(date +%s) - SWEEP_START_TS )))."
	[ "$FAILED" -eq 0 ]
	exit
fi

if [ -z "$ENV_NAME" ]; then
	prompt_for_device
fi

PORT_ARGS=()
MONITOR_PORT_ARGS=()
if [ -n "$PORT" ]; then
	PORT_ARGS=(--upload-port "$PORT")
	MONITOR_PORT_ARGS=(--port "$PORT")
fi

UPLOAD_TARGET="upload"
UPLOAD_LABEL="Upload"
if [ "$ERASE_FIRST" = true ]; then
	UPLOAD_TARGET="upload_erase"
	UPLOAD_LABEL="Erase flash and upload"
fi

BUILD_START_TS="$(date +%s)"
if [ "$FULLCLEAN" = true ]; then
	echo "[PIO] Full clean ($ENV_NAME)..."
	pio run -e "$ENV_NAME" -t fullclean
fi
echo "[PIO] $UPLOAD_LABEL ($ENV_NAME)..."
pio run -e "$ENV_NAME" -t "$UPLOAD_TARGET" ${PORT_ARGS[@]+"${PORT_ARGS[@]}"}
echo "[PIO] Build and upload took $(format_duration $(( $(date +%s) - BUILD_START_TS )))."

ELF_PATH=".pio/build/${ENV_NAME}/firmware.elf"
BIN_PATH=".pio/build/${ENV_NAME}/firmware.bin"
if [ -f "$ELF_PATH" ]; then
	ELF_SHA="$(shasum -a 256 "$ELF_PATH" | awk '{print $1}')"
	echo "[PIO] ELF SHA256: $ELF_SHA"
	echo "[PIO] Runtime monitor should show: ELF file SHA256: ${ELF_SHA:0:16}"
fi
if [ -f "$BIN_PATH" ]; then
	echo "[PIO] BIN SHA256: $(shasum -a 256 "$BIN_PATH" | awk '{print $1}')"
fi

echo "[PIO] Monitor ($ENV_NAME)..."
pio device monitor -e "$ENV_NAME" ${MONITOR_PORT_ARGS[@]+"${MONITOR_PORT_ARGS[@]}"}
