#!/bin/bash
# Cuts a monster-cyberspace release, or just builds the release images into
# dist/. Ported from platy-aprs's scripts/release.sh (itself from camillia-mt's).
#
# Run with --help (or no arguments) for the modes and options.
#
# Bash 3.2 (macOS's stock bash): no associative arrays, no mapfile.
set -euo pipefail

RELEASE_ENVS=(
    m9
    wio_l2
    tdeck
    crowpanel35
    crowpanel50
    pager
    tdeck_pro
    mesh_deck
)

# Every release image is merged with this esptool. esptool 5 renamed merge_bin
# and its flags, so an unpinned upgrade could quietly change a factory image's
# layout. The release workflow pins the same version.
ESPTOOL_VERSION="4.11.0"

# OTA signing. Each -ota.bin gets a detached ECDSA P-256 signature (.sig) that
# devices check against the public key baked into the firmware before they
# boot an update. The private key stays out of git: ota_signing_key.pem here,
# or the OTA_SIGNING_KEY secret of the `release` environment in CI.
SIGNING_KEY="ota_signing_key.pem"
PUBKEY_HEADER="src/app/ota_signing_pubkey.h"
SIGN_ASSETS=false

cd "$(git rev-parse --show-toplevel)"

env_label() {
    case "$1" in
        m9)          echo "Elecrow ThinkNode M9" ;;
        wio_l2)      echo "Seeed Wio Tracker L2" ;;
        tdeck)       echo "LilyGO T-Deck Plus" ;;
        crowpanel35) echo "Elecrow CrowPanel Advance 3.5\"" ;;
        crowpanel50) echo "Elecrow CrowPanel Advance 5.0\"" ;;
        pager)       echo "LilyGO T-LoRa Pager" ;;
        tdeck_pro)   echo "LilyGO T-Deck Pro" ;;
        mesh_deck)   echo "Attaky Mesh Deck" ;;
        *)           echo "$1" ;;
    esac
}

# Every board here is an ESP32-S3 with 16 MB of flash. Kept as functions so a
# board that differs only has to add a case. Artifact names use dashes.
env_out_name()   { echo "${1//_/-}"; }
env_chip()       { echo "esp32s3"; }
env_flash_size() { echo "16MB"; }
# DIO in the image header boots on every QIO-capable part, and is what the
# platy-aprs and camillia-mt images for the same boards ship with.
env_flash_mode() { echo "dio"; }

has_env() { grep -q "^\[env:$1\]" platformio.ini; }

validate_release_targets() {
    local env_name out_name seen="|" failed=false
    echo "Release target contract:"
    for env_name in "${RELEASE_ENVS[@]}"; do
        if ! has_env "$env_name"; then
            echo "  ERROR: release environment '$env_name' is missing from platformio.ini" >&2
            failed=true
            continue
        fi
        out_name="$(env_out_name "$env_name")"
        case "$seen" in
            *"|$out_name|"*)
                echo "  ERROR: duplicate release artifact slug '$out_name'" >&2
                failed=true
                ;;
            *) seen="$seen$out_name|" ;;
        esac
        if ! sed -n "/^\[env:$env_name\]/,/^\[/p" platformio.ini | grep -q 'set_app_version.py'; then
            echo "  ERROR: [env:$env_name] does not run tools/set_app_version.py" >&2
            failed=true
        fi
        printf '  %-12s -> monster-cyberspace-%s-<tag>.bin\n' "$env_name" "$out_name"
    done
    # Every firmware env should be released; a new board that is not is a slip.
    local e
    for e in $(sed -n 's/^\[env:\(.*\)\]$/\1/p' platformio.ini | grep -v '^native$'); do
        case " ${RELEASE_ENVS[*]} " in
            *" $e "*) ;;
            *) echo "  ERROR: [env:$e] is not in RELEASE_ENVS (scripts/release.sh)" >&2; failed=true ;;
        esac
    done
    if [[ ! -s "$PUBKEY_HEADER" ]] || [[ -z "$(header_pubkey)" ]]; then
        echo "  ERROR: $PUBKEY_HEADER is missing or holds no key" >&2
        failed=true
    fi
    [[ "$failed" == false ]]
}

# Base64 body of a PEM public key, for comparing keys.
pem_body() { grep -v -- '-----' | tr -d '\n'; }
header_pubkey() { grep -oE '"[A-Za-z0-9+/=]+' "$PUBKEY_HEADER" | grep -v BEGIN | tr -d '"' | tr -d '\n'; }

# Decides whether this run signs ($1 = required|optional). The header is baked
# into every image and devices check each later update against it, so it must
# be the committed one, and a key here must be its private half: built from
# any other key, devices would refuse every official update until reflashed.
check_signing() {
    if ! git diff --quiet HEAD -- "$PUBKEY_HEADER"; then
        echo "Error: $PUBKEY_HEADER differs from the committed copy. Restore it:" >&2
        echo "  git checkout -- $PUBKEY_HEADER" >&2
        exit 1
    fi
    if [[ ! -f "$SIGNING_KEY" ]]; then
        if [[ "$1" == required ]]; then
            echo "Error: $SIGNING_KEY not found; a release must sign its OTA images." >&2
            exit 1
        fi
        SIGN_ASSETS=false
        return 0
    fi
    command -v openssl >/dev/null 2>&1 || { echo "Error: openssl is required to sign." >&2; exit 1; }
    local fresh
    fresh="$(openssl ec -in "$SIGNING_KEY" -pubout 2>/dev/null | pem_body)"
    if [[ -z "$fresh" || "$fresh" != "$(header_pubkey)" ]]; then
        echo "Error: $SIGNING_KEY is not the key $PUBKEY_HEADER was made from." >&2
        echo "Devices would reject OTA images signed with it." >&2
        exit 1
    fi
    SIGN_ASSETS=true
}

find_pio() {
    if command -v pio >/dev/null 2>&1; then command -v pio
    elif [[ -x ".pio-core/penv/bin/pio" ]]; then echo "$PWD/.pio-core/penv/bin/pio"
    elif [[ -x "$HOME/.platformio/penv/bin/pio" ]]; then echo "$HOME/.platformio/penv/bin/pio"
    else return 1; fi
}

# Points ESPTOOL at esptool $ESPTOOL_VERSION: the one on PATH when it is that
# version (CI pins it), else a venv of its own, installed on first use.
setup_esptool() {
    if python3 -c "import esptool,sys; sys.exit(esptool.__version__ != '$ESPTOOL_VERSION')" 2>/dev/null; then
        ESPTOOL=( python3 -m esptool )
        return 0
    fi
    local venv="${XDG_CACHE_HOME:-$HOME/.cache}/monster-cyberspace/esptool-$ESPTOOL_VERSION"
    local have=""
    if [[ -x "$venv/bin/python" ]]; then
        have="$("$venv/bin/python" -c 'import esptool; print(esptool.__version__)' 2>/dev/null || true)"
    fi
    if [[ "$have" != "$ESPTOOL_VERSION" ]]; then
        echo "Installing esptool $ESPTOOL_VERSION into $venv"
        rm -rf "$venv"
        mkdir -p "$(dirname "$venv")"
        if ! python3 -m venv "$venv" \
           || ! "$venv/bin/python" -m pip install --quiet --disable-pip-version-check \
                "esptool==$ESPTOOL_VERSION"; then
            rm -rf "$venv"
            echo "Error: could not install esptool $ESPTOOL_VERSION (a one-time download from PyPI)." >&2
            exit 1
        fi
    fi
    ESPTOOL=( "$venv/bin/python" -m esptool )
}

# ── Build, merge and verify ──────────────────────────────────────────────────
# Shared by the release and --dist-only, so a dist/ built without releasing
# comes out exactly as a release's would.

build_release_envs() {
    local env_name boot_app0
    # The clean is so a release image can never contain a stale object file.
    # --no-clean trades that away for speed; fine on a fresh checkout (CI) or
    # right after building the same sources.
    if [[ "$NO_CLEAN" == true ]]; then
        echo "Skipping full clean (--no-clean)."
    else
        for env_name in "$@"; do
            "$PIO" run -e "$env_name" -t fullclean >/dev/null
        done
    fi
    # --disable-auto-clean: PlatformIO otherwise wipes every env's build output
    # when the project checksum moves between two `pio run`s, and the merge
    # below would find nothing to merge.
    for env_name in "$@"; do
        echo "==> $(env_label "$env_name") [$env_name]"
        "$PIO" run -e "$env_name" --disable-auto-clean
        # platformio.ini keeps its own core_dir (.pio-core), not ~/.platformio.
        boot_app0="$(find "${PLATFORMIO_CORE_DIR:-.pio-core}/packages/framework-arduinoespressif32/tools/partitions" \
            -name boot_app0.bin 2>/dev/null | head -1)"
        if [[ -z "$boot_app0" ]]; then
            echo "Error: boot_app0.bin missing after building $env_name." >&2
            exit 1
        fi
        cp "$boot_app0" ".pio/build/$env_name/boot_app0.bin"
    done
}

# dist/monster-cyberspace-<board>-<tag>.bin          factory image, flash at 0x0
#                                                    (settings live past it: kept)
# dist/monster-cyberspace-<board>-<tag>-ota.bin      app only, what devices download
# dist/monster-cyberspace-<board>-<tag>-ota.bin.sig  its ECDSA P-256 signature
# dist/monster-cyberspace-<board>-<tag>.elf          symbols for crash decoding (not published)
merge_assets() {
    local tag="$1" env_name d out
    shift
    for env_name in "$@"; do
        d=".pio/build/$env_name"
        out="dist/monster-cyberspace-$(env_out_name "$env_name")-$tag"
        echo "  $env_name -> $out.bin"
        "${ESPTOOL[@]}" --chip "$(env_chip "$env_name")" merge_bin -o "$out.bin" \
            -fm "$(env_flash_mode "$env_name")" -ff 80m -fs "$(env_flash_size "$env_name")" \
            0x0     "$d/bootloader.bin" \
            0x8000  "$d/partitions.bin" \
            0xe000  "$d/boot_app0.bin" \
            0x10000 "$d/firmware.bin" >/dev/null
        cp "$d/firmware.bin" "$out-ota.bin"
        cp "$d/firmware.elf" "$out.elf"
        if [[ "$SIGN_ASSETS" == true ]]; then
            openssl dgst -sha256 -sign "$SIGNING_KEY" -out "$out-ota.bin.sig" "$out-ota.bin"
        fi
    done
    install -m 0755 scripts/flash.sh dist/flash.sh
}

verify_assets() {
    local tag="$1" env_name out asset
    shift
    echo "Verifying release assets..."
    for env_name in "$@"; do
        out="dist/monster-cyberspace-$(env_out_name "$env_name")-$tag"
        for asset in "$out.bin" "$out-ota.bin"; do
            if [[ ! -s "$asset" ]]; then
                echo "Error: missing or empty release asset: $asset" >&2
                return 1
            fi
        done
        if ! cmp -s ".pio/build/$env_name/firmware.bin" "$out-ota.bin"; then
            echo "Error: $out-ota.bin does not match $env_name/firmware.bin" >&2
            return 1
        fi
        if [[ "$SIGN_ASSETS" != true ]]; then
            echo "  OK $env_name: $(basename "$out.bin"), $(basename "$out-ota.bin") (unsigned)"
            continue
        fi
        if ! openssl dgst -sha256 -verify <(openssl ec -in "$SIGNING_KEY" -pubout 2>/dev/null) \
                -signature "$out-ota.bin.sig" "$out-ota.bin" >/dev/null 2>&1; then
            echo "Error: signature check failed: $out-ota.bin.sig" >&2
            return 1
        fi
        echo "  OK $env_name: $(basename "$out.bin"), $(basename "$out-ota.bin"), $(basename "$out-ota.bin.sig")"
    done
    [[ -x dist/flash.sh ]] || { echo "Error: dist/flash.sh missing" >&2; return 1; }
}

# BUILD-INFO.txt says which build someone was given; SHA256SUMS is for
# wherever the images end up.
write_build_info() {
    local version="$1" dirty="$2"
    shift 2
    {
        echo "monster-cyberspace $version"
        echo "Built:   $(date -u '+%Y-%m-%d %H:%M UTC')"
        echo "Commit:  $(git rev-parse HEAD) ($(git rev-parse --abbrev-ref HEAD))"
        echo "Boards:  $*"
        echo "Signed:  $([[ "$SIGN_ASSETS" == true ]] && echo yes || echo no)"
        if [[ -n "$dirty" ]]; then
            echo ""
            echo "Built with uncommitted changes:"
            printf '%s\n' "$dirty" | sed 's/^/  /'
        fi
    } > dist/BUILD-INFO.txt
    if command -v shasum >/dev/null 2>&1; then
        (cd dist && shasum -a 256 -- *.bin) > dist/SHA256SUMS
    else
        (cd dist && sha256sum -- *.bin) > dist/SHA256SUMS
    fi
}

# ── Flags ────────────────────────────────────────────────────────────────────
MODE=""                # release | dist | notes | check
ASSUME_YES=false
VERSION_ARG=""
NO_CLEAN=false
BUILD_LOCAL=false
USE_COMMITTED_NOTES=false
FROM_MAIN=false
ONLY_ARG=""

usage() {
    local b="" u="" r=""
    if [[ -t 1 ]]; then b=$'\e[1m'; u=$'\e[4m'; r=$'\e[0m'; fi
    cat <<EOF
${b}monster-cyberspace release tool${r}

${b}USAGE${r}
  ./scripts/release.sh --release [-y] [--version V] [--use-committed-notes] [--build-local] [--no-clean]
  ./scripts/release.sh --from-main [-y] [--version V] [--use-committed-notes]
  ./scripts/release.sh --dist-only [--only ENVS] [--version V] [--no-clean]
  ./scripts/release.sh --notes-only
  ./scripts/release.sh --check-targets
  ./scripts/release.sh --help

${b}MODES${r}
  ${u}--release${r}
      Release main as committed. Checks the tree is clean and in sync with
      origin, picks the version, then dispatches .github/workflows/release.yml,
      which builds every board, commits VERSION, tags v<version> and publishes
      the images. Nothing is compiled on this machine. -y or --version on
      their own imply --release.

  ${u}--from-main${r}
      Release origin/main as it stands on GitHub, whatever this clone has
      checked out: another branch, uncommitted changes or unpushed commits are
      all fine and none of them ship. Fetches origin, picks the version from
      its tags, and dispatches the workflow on main. Takes the release
      options except --build-local and --no-clean.

  ${u}--dist-only${r}
      Build the release images into dist/ and release nothing: no commit, tag,
      push or GitHub release. Builds the working tree as it stands, uncommitted
      changes included. The images carry VERSION on the tagged release commit,
      else VERSION+<commit> (plus .custom with uncommitted changes).

  ${u}--notes-only${r}
      Draft RELEASE_NOTES.md from the commits since the last tag and open it
      in \$EDITOR. Commit it, then release with --use-committed-notes.

  ${u}--check-targets${r}
      Validate the release environments and artifact names, then exit.

${b}RELEASE OPTIONS${r}
  -y, --yes               Bump the patch of the latest tag without prompting
                          (v0.1.3 -> v0.1.4; VERSION as it reads when there is
                          no tag yet).
  --version V             Release exactly this version, e.g. 0.2.0 (a leading
                          'v' is fine).
  --use-committed-notes   Publish RELEASE_NOTES.md verbatim instead of GitHub's
                          generated notes.
  --build-local           Build, commit, tag and publish from this machine
                          instead of dispatching the workflow. Needs PlatformIO
                          and an authenticated gh. The workflow's own run
                          works this way.
  --no-clean              Skip the full clean and build on the existing .pio
                          output. Faster; only as trustworthy as that tree.

${b}DIST-ONLY OPTIONS${r}
  --only ENVS             Build just these boards, comma-separated (may be
                          repeated; built in release order).
                          Boards: ${RELEASE_ENVS[*]}
  --version V             Name the images after V instead (e.g. 0.2.0+test).
                          File names drop the +metadata, as on a release.
  --no-clean              As above.

${b}OUTPUT${r} (dist/)
  monster-cyberspace-<board>-vX.Y.Z.bin          factory image, flash at 0x0 (keeps settings)
  monster-cyberspace-<board>-vX.Y.Z-ota.bin      app only, what devices download over the air
  monster-cyberspace-<board>-vX.Y.Z-ota.bin.sig  its ECDSA P-256 signature (needs
                                                 ota_signing_key.pem here; a device will
                                                 not install an unsigned image)
  monster-cyberspace-<board>-vX.Y.Z.elf          symbols for decoding crashes (not published)
  flash.sh  BUILD-INFO.txt  SHA256SUMS

${b}EXAMPLES${r}
  ./scripts/release.sh --dist-only
  ./scripts/release.sh --dist-only --only m9,pager --no-clean
  ./scripts/release.sh --notes-only && git commit -am "Release notes"
  ./scripts/release.sh -y --use-committed-notes
  ./scripts/release.sh --release --version 0.2.0
  ./scripts/release.sh --from-main -y
EOF
}

set_mode() {
    if [[ -n "$MODE" && "$MODE" != "$1" ]]; then
        echo "Choose one of --release (or --from-main), --dist-only, --notes-only, --check-targets." >&2
        exit 1
    fi
    MODE="$1"
}

if [[ $# -eq 0 ]]; then usage; exit 0; fi
while [[ $# -gt 0 ]]; do
    case "$1" in
        -h|--help) usage; exit 0 ;;
        --release) set_mode release ;;
        --from-main) set_mode release; FROM_MAIN=true ;;
        --dist-only) set_mode dist ;;
        --notes-only) set_mode notes ;;
        --check-targets) set_mode check ;;
        -y|--yes) ASSUME_YES=true ;;
        --no-clean) NO_CLEAN=true ;;
        --build-local) BUILD_LOCAL=true ;;
        --use-committed-notes) USE_COMMITTED_NOTES=true ;;
        --only)
            [[ -n "${2:-}" ]] || { echo "--only requires environments (e.g. --only m9,pager)" >&2; exit 1; }
            ONLY_ARG="${ONLY_ARG:+$ONLY_ARG,}$2"
            shift
            ;;
        --only=*) ONLY_ARG="${ONLY_ARG:+$ONLY_ARG,}${1#*=}" ;;
        --version)
            [[ -n "${2:-}" ]] || { echo "--version requires a value (e.g. --version 0.2.0)" >&2; exit 1; }
            VERSION_ARG="$2"
            shift
            ;;
        --version=*) VERSION_ARG="${1#*=}" ;;
        *) echo "Unknown argument: $1 (see --help)" >&2; exit 1 ;;
    esac
    shift
done

# -y and --version on their own mean a release.
if [[ -z "$MODE" ]]; then
    if [[ "$ASSUME_YES" == true || -n "$VERSION_ARG" ]]; then MODE=release
    else usage >&2; exit 1; fi
fi

reject() { echo "$1 does not take $2 (see --help)." >&2; exit 1; }
case "$MODE" in
    dist)
        if [[ "$ASSUME_YES" == true ]]; then reject --dist-only -y; fi
        if [[ "$BUILD_LOCAL" == true ]]; then reject --dist-only --build-local; fi
        if [[ "$USE_COMMITTED_NOTES" == true ]]; then reject --dist-only --use-committed-notes; fi
        ;;
    release)
        if [[ -n "$ONLY_ARG" ]]; then reject --release --only; fi
        if [[ "$FROM_MAIN" == true ]]; then
            if [[ "$BUILD_LOCAL" == true ]]; then reject --from-main --build-local; fi
            if [[ "$NO_CLEAN" == true ]]; then reject --from-main --no-clean; fi
        fi
        ;;
    notes|check)
        if [[ "$ASSUME_YES" == true || -n "$VERSION_ARG" || "$NO_CLEAN" == true || "$BUILD_LOCAL" == true \
              || "$USE_COMMITTED_NOTES" == true || -n "$ONLY_ARG" ]]; then
            echo "--notes-only and --check-targets take no options." >&2
            exit 1
        fi
        ;;
esac

# ── --check-targets ──────────────────────────────────────────────────────────
if [[ "$MODE" == check ]]; then
    validate_release_targets
    exit $?
fi

validate_release_targets >/dev/null || { validate_release_targets; exit 1; }

# ── --notes-only ─────────────────────────────────────────────────────────────
if [[ "$MODE" == notes ]]; then
    prev="$(git describe --tags --abbrev=0 2>/dev/null || true)"
    range="${prev:+$prev..}HEAD"
    {
        echo "### Changes"
        git log --no-merges --format='- %s' "$range" | grep -v '\[skip ci\]' || true
    } > RELEASE_NOTES.md
    "${EDITOR:-vi}" RELEASE_NOTES.md
    echo "RELEASE_NOTES.md written (commits ${prev:-since the start})."
    echo "Commit it, then release with --use-committed-notes."
    exit 0
fi

# ── --dist-only ──────────────────────────────────────────────────────────────
if [[ "$MODE" == dist ]]; then
    DIST_ENVS=()
    if [[ -z "$ONLY_ARG" ]]; then
        DIST_ENVS=( "${RELEASE_ENVS[@]}" )
    else
        IFS=',' read -r -a requested <<< "$ONLY_ARG"
        for want in "${requested[@]}"; do
            case " ${RELEASE_ENVS[*]} " in
                *" $want "*) ;;
                *) echo "--only: '$want' is not a release environment. Choose from: ${RELEASE_ENVS[*]}" >&2; exit 1 ;;
            esac
        done
        for env_name in "${RELEASE_ENVS[@]}"; do
            for want in "${requested[@]}"; do
                if [[ "$want" == "$env_name" ]]; then DIST_ENVS+=( "$env_name" ); break; fi
            done
        done
    fi

    # Read before VERSION is touched, so the run's own edit never counts.
    dirty="$(git status --porcelain)"
    tagged=""
    if [[ -n "$VERSION_ARG" ]]; then
        DIST_TAG="v${VERSION_ARG#v}"
    else
        base="$(tr -d '[:space:]' < VERSION)"
        base="v${base#v}"
        base="${base%%+*}"
        tagged="$(git rev-parse -q --verify "refs/tags/$base^{commit}" 2>/dev/null || true)"
        if [[ -n "$dirty" ]]; then DIST_TAG="$base+$(git rev-parse --short=7 HEAD).custom"
        elif [[ "$tagged" == "$(git rev-parse HEAD)" ]]; then DIST_TAG="$base"
        else DIST_TAG="$base+$(git rev-parse --short=7 HEAD)"; fi
    fi
    if [[ ! "$DIST_TAG" =~ ^v[0-9]+\.[0-9]+\.[0-9]+(\+[0-9A-Za-z.]+)?$ ]]; then
        echo "Error: '$DIST_TAG' is not a version to build (X.Y.Z, optionally +build)." >&2
        exit 1
    fi
    FILE_TAG="${DIST_TAG%%+*}"

    PIO="$(find_pio)" || { echo "Error: platformio not found. Run: pip install platformio" >&2; exit 1; }
    check_signing optional
    setup_esptool

    echo ""
    echo "Dist-only build: nothing is committed, tagged or published."
    echo "  Version:  $DIST_TAG"
    echo "  Files:    monster-cyberspace-<board>-$FILE_TAG.bin"
    echo "  Commit:   $(git rev-parse --short HEAD) ($(git rev-parse --abbrev-ref HEAD))"
    if [[ -n "$dirty" ]]; then echo "  Uncommitted changes, built in:"; printf '%s\n' "$dirty" | sed 's/^/    /'; fi
    echo "  Boards:   ${DIST_ENVS[*]}"
    if [[ "$SIGN_ASSETS" == true ]]; then echo "  Signing:  $SIGNING_KEY"
    else echo "  Signing:  none ($SIGNING_KEY is not here, so no .sig files)"; fi
    echo "  Output:   dist/, replaced"
    if [[ -n "$VERSION_ARG" && "$DIST_TAG" != *+* ]]; then
        echo ""
        echo "Note: $DIST_TAG has no +build metadata, so these images will call"
        echo "themselves $DIST_TAG without necessarily being that release."
    fi
    echo ""

    # VERSION is baked into the firmware, so it is rewritten for the build and
    # put back however the run ends.
    DIST_TMP="$(mktemp -d "${TMPDIR:-/tmp}/monster-dist.XXXXXX")"
    cp -p VERSION "$DIST_TMP/VERSION"
    trap 'cp -p "$DIST_TMP/VERSION" VERSION; rm -rf "$DIST_TMP"' EXIT
    echo "$DIST_TAG" > VERSION

    build_release_envs "${DIST_ENVS[@]}"
    echo ""
    echo "Merging factory images..."
    rm -rf dist
    mkdir -p dist
    merge_assets "$FILE_TAG" "${DIST_ENVS[@]}"
    verify_assets "$FILE_TAG" "${DIST_ENVS[@]}"
    write_build_info "$DIST_TAG" "$dirty" "${DIST_ENVS[@]}"
    echo ""
    ls -lh dist/
    echo ""
    echo "dist/ holds $DIST_TAG for ${#DIST_ENVS[@]} board(s). Nothing was committed, tagged or published."
    exit 0
fi

# ── Release ──────────────────────────────────────────────────────────────────
# Without --build-local this machine only checks and dispatches; the workflow
# (GITHUB_ACTIONS) is the run that builds.
REMOTE=true
if [[ "$BUILD_LOCAL" == true || "${GITHUB_ACTIONS:-}" == "true" ]]; then
    REMOTE=false
fi

command -v gh >/dev/null 2>&1 || { echo "Error: the GitHub CLI (gh) is required to release." >&2; exit 1; }

if [[ "$FROM_MAIN" == true ]]; then
    # origin/main is what the workflow checks out; this clone's state is moot.
    BRANCH=main
    git fetch -q --tags origin main
    echo "Releasing origin/main at $(git log -1 --format='%h %s' origin/main)"
    if git rev-parse -q --verify refs/heads/main >/dev/null; then
        unpushed="$(git rev-list --count origin/main..main)"
        if [[ "$unpushed" -gt 0 ]]; then
            echo "Note: local main has $unpushed unpushed commit(s); they are not in this release."
        fi
    fi
elif [[ "${GITHUB_ACTIONS:-}" != "true" ]]; then
    # A release ships only committed work.
    dirty="$(git status --porcelain)"
    if [[ -n "$dirty" ]]; then
        echo "The working tree has uncommitted changes:" >&2
        printf '%s\n' "$dirty" | sed 's/^/  /' >&2
        echo "A release ships only committed work. Commit or stash it and rerun." >&2
        exit 1
    fi
    BRANCH="$(git rev-parse --abbrev-ref HEAD)"
    git fetch -q --tags origin
    if [[ "$(git rev-parse HEAD)" != "$(git rev-parse "origin/$BRANCH" 2>/dev/null || true)" ]]; then
        echo "$BRANCH is not in sync with origin/$BRANCH. Push or pull first." >&2
        exit 1
    fi
fi
if [[ "$FROM_MAIN" == true && "$USE_COMMITTED_NOTES" == true ]]; then
    if [[ -z "$(git show origin/main:RELEASE_NOTES.md 2>/dev/null)" ]]; then
        echo "--use-committed-notes: RELEASE_NOTES.md is missing or empty on origin/main." >&2
        exit 1
    fi
elif [[ "$USE_COMMITTED_NOTES" == true && ! -s RELEASE_NOTES.md ]]; then
    echo "--use-committed-notes: RELEASE_NOTES.md is missing or empty (draft it with --notes-only)." >&2
    exit 1
fi

# Newest well-formed tag, in version order.
PREV_TAG="$(git tag --list 'v*' | grep -E '^v[0-9]+\.[0-9]+\.[0-9]+$' | sort -V | tail -1 || true)"
if [[ "$FROM_MAIN" == true ]]; then
    if ! CURRENT="$(git show origin/main:VERSION 2>/dev/null | tr -d '[:space:]')" || [[ -z "$CURRENT" ]] \
          || ! git cat-file -e origin/main:.github/workflows/release.yml 2>/dev/null; then
        echo "origin/main has no release tooling (VERSION, release.yml) yet. Push it first." >&2
        exit 1
    fi
else
    CURRENT="$(tr -d '[:space:]' < VERSION)"
fi
echo "VERSION:     $CURRENT"
echo "Latest tag:  ${PREV_TAG:-none}"

if [[ -n "$VERSION_ARG" ]]; then
    VERSION="${VERSION_ARG#v}"
elif [[ "$ASSUME_YES" == true ]]; then
    if [[ "$PREV_TAG" =~ ^v([0-9]+)\.([0-9]+)\.([0-9]+)$ ]]; then
        VERSION="${BASH_REMATCH[1]}.${BASH_REMATCH[2]}.$(( BASH_REMATCH[3] + 1 ))"
    else
        base="${CURRENT#v}"
        VERSION="${base%%[-+]*}"   # first release: VERSION as it reads
    fi
else
    read -rp "New version (e.g. 0.2.0): " VERSION
fi
VERSION="${VERSION#v}"
if [[ ! "$VERSION" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
    echo "Version must look like 0.2.0 (got '$VERSION')." >&2
    exit 1
fi
TAG="v$VERSION"
if git rev-parse -q --verify "refs/tags/$TAG" >/dev/null; then
    echo "Tag $TAG already exists." >&2
    exit 1
fi
echo "Releasing $TAG"

if [[ "$REMOTE" == true ]]; then
    notes=generate
    if [[ "$USE_COMMITTED_NOTES" == true ]]; then notes=committed; fi
    gh workflow run release.yml --ref "$BRANCH" -f version="$VERSION" -f notes="$notes"
    echo "Release workflow dispatched for $TAG (notes: $notes). Follow it with: gh run watch"
    exit 0
fi

PIO="$(find_pio)" || { echo "Error: platformio not found." >&2; exit 1; }
check_signing required
setup_esptool

echo "$TAG" > VERSION
build_release_envs "${RELEASE_ENVS[@]}"

echo ""
echo "Merging factory images..."
rm -rf dist
mkdir -p dist
merge_assets "$TAG" "${RELEASE_ENVS[@]}"
verify_assets "$TAG" "${RELEASE_ENVS[@]}"
write_build_info "$TAG" "" "${RELEASE_ENVS[@]}"

# [skip ci]: this tree was just built, so build.yml building it again on the
# push would tell us nothing.
git add VERSION
if git diff --cached --quiet; then
    echo "VERSION already reads $TAG."
else
    git commit -q -m "Release $TAG [skip ci]"
fi
git push -q origin HEAD
git tag "$TAG"
git push -q origin "$TAG"
echo "Tag $TAG pushed."

NOTES_ARGS=( --generate-notes )
if [[ "$USE_COMMITTED_NOTES" == true ]]; then NOTES_ARGS=( --notes-file RELEASE_NOTES.md ); fi

# The .elf files stay out of the release; the workflow archives them.
gh release create "$TAG" --title "monster-cyberspace $TAG" "${NOTES_ARGS[@]}" \
    dist/*.bin dist/*.sig dist/SHA256SUMS dist/flash.sh
echo "Release $TAG published."
gh release view "$TAG" --json url -q .url
