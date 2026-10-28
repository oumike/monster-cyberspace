# Building

## Prerequisites

- [PlatformIO Core](https://platformio.org/install/cli) 6.1 or newer. The first
  build downloads the pioarduino platform (Arduino core 3.3 / ESP-IDF 5.5)
  into `.pio-core/`, kept apart from the shared `~/.platformio`.
- A USB data cable. The ThinkNode M9's console runs through its USB-to-UART
  bridge at 115200 baud, and so do the CrowPanels' (a CH340); the Wio Tracker
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
| `native` | Host unit tests for `lib/core` |

```sh
pio test -e native                    # unit tests on your computer
pio run -e m9                         # build
pio run -e wio_l2                     # build for the Wio Tracker L2
pio run -e tdeck                      # build for the T-Deck Plus
pio run -e crowpanel35                # build for the CrowPanel Advance 3.5"
pio run -e crowpanel50                # build for the CrowPanel Advance 5.0"
pio run -e pager                      # build for the T-LoRa Pager
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
| `ENV` | Or name any firmware env directly; with neither, a menu lists every firmware env |
| `--port`/`-p PORT` | Serial port for upload and monitor (default: auto-detect) |
| `--erase`/`-E` | Erase the whole flash in the same esptool session as the upload (`upload_erase` target); wipes saved settings |
| `--fullclean`/`-F` | `pio run -t fullclean` first |
| `--just-build`/`-B` | Compile only, every firmware env or the one named; prints a pass/fail summary |

If a first build fails while installing a tool package, run it again. A
one-off PlatformIO install error has been seen on the first run.

## Releases

Ported from platy-aprs, without OTA signing (this firmware has no over-the-air updates).

```sh
scripts/release.sh                             # help: every mode and option
scripts/release.sh --dist-only                 # build release images for every board into dist/, release nothing
scripts/release.sh --dist-only --only m9,pager --no-clean
scripts/release.sh --notes-only                # draft RELEASE_NOTES.md from the commits since the last tag
scripts/release.sh -y --use-committed-notes    # release: bump the patch version, dispatch the Release workflow
scripts/release.sh --release --version 0.2.0   # release an exact version
```

`VERSION` (e.g. `v0.1.0`) is baked into every firmware build as `RC_VERSION` by
`tools/set_app_version.py`. `dist/` holds, per board,
`monster-cyberspace-<board>-vX.Y.Z.bin` (factory image, flash at `0x0`; wipes
settings), `-app.bin` (app image, flash at `0x10000`; keeps settings) and `.elf`
(symbols), plus `flash.sh`, `BUILD-INFO.txt` and `SHA256SUMS`. A dist-only build
is versioned from `VERSION`, with `+<commit>` (and `.custom` for uncommitted
changes) unless it is the tagged release commit.

A release ships only committed work on a branch in sync with origin. The script
picks the version and dispatches `.github/workflows/release.yml`, which builds
every board, commits `VERSION`, tags `v<version>`, publishes the images, and
keeps the `.elf` files as a workflow artifact. `--build-local` does the same on
your machine. `.github/workflows/build.yml` builds every board from a clean
checkout on each push and pull request and runs the unit tests; it publishes
nothing. A new firmware env must be added to `RELEASE_ENVS` in
`scripts/release.sh` and run `tools/set_app_version.py`; CI checks both.

## Time zones

The time zone is set by hand. The zone list (`lib/core/src/tz/zones.cpp`) is
curated: every inhabited UTC offset at least once, plus the larger population
centres. Each POSIX string is copied from the footer of the matching file in
`/usr/share/zoneinfo` (the last line of the TZif file).
