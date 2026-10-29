# Building

## Prerequisites

- [PlatformIO Core](https://platformio.org/install/cli) 6.1 or newer. The first
  build downloads the pioarduino platform (Arduino core 3.3 / ESP-IDF 5.5)
  into `.pio-core/`, kept apart from the shared `~/.platformio`.
- A USB data cable. The ThinkNode M9's console runs through its USB-to-UART
  bridge at 115200 baud, and so do the CrowPanels' and the Mesh Deck's (a CH340); the Wio Tracker
  L2's and the T-Deck Plus's run through the ESP32-S3's own USB.

## Environments

| Env | What it builds |
|---|---|
| `m9` | The firmware for the Elecrow ThinkNode M9 |
| `wio_l2` | The firmware for the Seeed Wio Tracker L2 (touch UI; not yet tried on hardware) |
| `tdeck` | The firmware for the LilyGO T-Deck Plus (keyboard, trackball, touch; not yet tried on hardware) |
| `crowpanel35` | The firmware for the Elecrow CrowPanel Advance 3.5" (touch only; not yet tried on hardware) |
| `crowpanel50` | The firmware for the Elecrow CrowPanel Advance 5.0" (touch only; not yet tried on hardware) |
| `pager` | The firmware for the LilyGO T-LoRa Pager (keyboard and side wheel, no touch; not yet tried on hardware) |
| `tdeck_pro` | The firmware for the LilyGO T-Deck Pro (e-paper, keyboard, touch; not yet tried on hardware) |
| `mesh_deck` | The firmware for the Attaky Mesh Deck (d-pad, touch, keyboard module, notification light; boots on hardware, the rest not yet checked) |
| `native` | Host unit tests for `lib/core` |

```sh
pio test -e native                    # unit tests on your computer
pio run -e m9                         # build
pio run -e wio_l2                     # build for the Wio Tracker L2
pio run -e tdeck                      # build for the T-Deck Plus
pio run -e crowpanel35                # build for the CrowPanel Advance 3.5"
pio run -e crowpanel50                # build for the CrowPanel Advance 5.0"
pio run -e pager                      # build for the T-LoRa Pager
pio run -e tdeck_pro                  # build for the T-Deck Pro
pio run -e mesh_deck                  # build for the Mesh Deck
scripts/build-upload-monitor.sh m9    # build, flash, then open the serial monitor
```

`scripts/build-upload-monitor.sh` options (`--help` lists them):

| Option | What it does |
|---|---|
| `--m9`/`-9` | Board flag: Elecrow ThinkNode M9 (`m9`) |
| `--wio-l2`/`-w` | Board flag: Seeed Wio Tracker L2 (`wio_l2`) |
| `--tdeck`/`-t` | Board flag: LilyGO T-Deck Plus (`tdeck`) |
| `--crowpanel35`/`-3` | Board flag: Elecrow CrowPanel Advance 3.5" (`crowpanel35`) |
| `--crowpanel50`/`-5` | Board flag: Elecrow CrowPanel Advance 5.0" (`crowpanel50`) |
| `--pager`/`-P` | Board flag: LilyGO T-LoRa Pager (`pager`) |
| `--tdeck-pro`/`-D` | Board flag: LilyGO T-Deck Pro (`tdeck_pro`) |
| `--mesh-deck`/`-M` | Board flag: Attaky Mesh Deck (`mesh_deck`) |
| `ENV` | Or name any firmware env directly; with neither, a menu lists every firmware env |
| `--port`/`-p PORT` | Serial port for upload and monitor (default: auto-detect) |
| `--erase`/`-E` | Erase the whole flash in the same esptool session as the upload (`upload_erase` target); wipes saved settings |
| `--fullclean`/`-F` | `pio run -t fullclean` first |
| `--just-build`/`-B` | Compile only, every firmware env or the one named; prints a pass/fail summary |

If a first build fails while installing a tool package, run it again. A
one-off PlatformIO install error has been seen on the first run.

## Releases

Ported from platy-aprs, with its OTA signing.

```sh
scripts/release.sh                             # help: every mode and option
scripts/release.sh --dist-only                 # build release images for every board into dist/, release nothing
scripts/release.sh --dist-only --only m9,pager --no-clean
scripts/release.sh --notes-only                # draft RELEASE_NOTES.md from the commits since the last tag
scripts/release.sh -y --use-committed-notes    # release: bump the patch version, dispatch the Release workflow
scripts/release.sh --release --version 0.2.0   # release an exact version
scripts/release.sh --from-main -y              # release origin/main as it is on GitHub, from any local state
```

`VERSION` (e.g. `v0.1.0`) is baked into every firmware build as `RC_VERSION` by
`tools/set_app_version.py`. `dist/` holds, per board,
`monster-cyberspace-<board>-vX.Y.Z.bin` (factory image, flash at `0x0`; keeps
settings), `-ota.bin` (the app image devices download), `-ota.bin.sig` (its
signature) and `.elf` (symbols), plus `flash.sh`, `BUILD-INFO.txt` and `SHA256SUMS`. A dist-only build
is versioned from `VERSION`, with `+<commit>` (and `.custom` for uncommitted
changes) unless it is the tagged release commit.

A release ships only committed work on a branch in sync with origin. The script
picks the version and dispatches `.github/workflows/release.yml`, which builds
every board, commits `VERSION`, tags `v<version>`, publishes the images, and
keeps the `.elf` files as a workflow artifact. `--build-local` does the same on
your machine. `--from-main` skips the local checks and releases `origin/main`
as pushed, whatever branch or changes this clone has; unpushed commits are not
included. `.github/workflows/build.yml` builds every board from a clean
checkout on each push and pull request and runs the unit tests; it publishes
nothing. A new firmware env must be added to `RELEASE_ENVS` in
`scripts/release.sh` and run `tools/set_app_version.py`; CI checks both. Its
`board::otaSlug()` must match the release's file name (`env_out_name`: the env
name with `_` turned into `-`), or its devices find no update.

The browser flasher (<https://oumike.github.io/monster-cyberspace/>) is
`site/index.html` (ESP Web Tools). `.github/workflows/pages.yml` publishes it
to GitHub Pages after each release: `scripts/pages-site.sh` copies the latest
release's factory images into the site (browsers can't fetch release assets
directly) and writes a manifest per board. A new board needs a name in the
page's `LABELS`. Pages must be set to deploy from GitHub Actions.

### Flash layout

Every env uses `partitions_16mb.csv`: two 6.25 MB app slots at `0x10000` and
`0x650000`, and the settings (`nvs`) at `0xC90000`, past anything a factory
image writes. `0x9000` is `nvs_old`, where the stock `default_16MB.csv` kept
them; `src/store/nvs_migrate.cpp` moves anything there into `nvs` at boot and
erases it. Keep the app slots where they are: the OTA code refuses to update a
board whose layout differs (`layoutSupportsUpdate`).

### OTA signing

Devices fetch `https://github.com/oumike/monster-cyberspace/releases/latest`
(the tag comes from its redirect), then `-ota.bin` and `-ota.bin.sig` from that
release, over HTTPS with the certificate bundle. The image is SHA-256'd as it
streams and its ECDSA P-256 signature checked against
`src/app/ota_signing_pubkey.h` before the slot is made bootable. A new image
that crashes before 30 s of uptime is rolled back by the bootloader.

The private key is `ota_signing_key.pem` (git-ignored) at the repo root, and
the `OTA_SIGNING_KEY` secret of the `release` environment for the workflow.
A release refuses to run without it, and `release.sh` and the workflow both
check it matches the committed header. **Keep a copy somewhere safe:** without
it no device in the field accepts another update until reflashed over USB. A
new key means a new header, and devices running the old one only take a USB
install. To make one:

```sh
openssl ecparam -name prime256v1 -genkey -noout -out ota_signing_key.pem
openssl ec -in ota_signing_key.pem -pubout   # paste into src/app/ota_signing_pubkey.h
gh secret set OTA_SIGNING_KEY --env release < ota_signing_key.pem
```

## Time zones

The time zone is set by hand. The zone list (`lib/core/src/tz/zones.cpp`) is
curated: every inhabited UTC offset at least once, plus the larger population
centres. Each POSIX string is copied from the footer of the matching file in
`/usr/share/zoneinfo` (the last line of the TZif file).
