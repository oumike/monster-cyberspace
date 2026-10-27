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
| `native` | Host unit tests for `lib/core` |

```sh
pio test -e native                    # unit tests on your computer
pio run -e m9                         # build
pio run -e wio_l2                     # build for the Wio Tracker L2
pio run -e tdeck                      # build for the T-Deck Plus
pio run -e crowpanel35                # build for the CrowPanel Advance 3.5"
pio run -e crowpanel50                # build for the CrowPanel Advance 5.0"
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
| `ENV` | Or name any firmware env directly; with neither, a menu lists every firmware env |
| `--port`/`-p PORT` | Serial port for upload and monitor (default: auto-detect) |
| `--erase`/`-E` | Erase the whole flash in the same esptool session as the upload (`upload_erase` target); wipes saved settings |
| `--fullclean`/`-F` | `pio run -t fullclean` first |
| `--just-build`/`-B` | Compile only, every firmware env or the one named; prints a pass/fail summary |

If a first build fails while installing a tool package, run it again. A
one-off PlatformIO install error has been seen on the first run.

## Time zones

The time zone is set by hand. The zone list (`lib/core/src/tz/zones.cpp`) is
curated: every inhabited UTC offset at least once, plus the larger population
centres. Each POSIX string is copied from the footer of the matching file in
`/usr/share/zoneinfo` (the last line of the TZif file).
