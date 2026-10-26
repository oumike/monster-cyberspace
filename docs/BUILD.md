# Building

## Prerequisites

- [PlatformIO Core](https://platformio.org/install/cli) 6.1 or newer. The first
  build downloads the pioarduino platform (Arduino core 3.3 / ESP-IDF 5.5).
- A USB data cable. The ThinkNode M9's console runs through its USB-to-UART
  bridge at 115200 baud; the Wio Tracker L2's through the ESP32-S3's own USB.

## Environments

| Env | What it builds |
|---|---|
| `m9` | The firmware for the Elecrow ThinkNode M9 |
| `m9_fake` | The same firmware with a scripted fake Cyberspace API, for trying every screen without an account |
| `wio_l2` | The firmware for the Seeed Wio Tracker L2 (hardware bring-up: the touch UI is still to come, so its two buttons stand in) |
| `wio_l2_fake` | The Wio Tracker L2 firmware with the fake API |
| `native` | Host unit tests for `lib/core` |

```sh
pio test -e native                    # unit tests on your computer
pio run -e m9                         # build
pio run -e wio_l2                     # build for the Wio Tracker L2
scripts/build-upload-monitor.sh m9    # build, flash, then open the serial monitor
```

`scripts/build-upload-monitor.sh` options (`--help` lists them):

| Option | What it does |
|---|---|
| `--m9`/`-9` | Board flag: Elecrow ThinkNode M9 (`m9`) |
| `--wio-l2`/`-w` | Board flag: Seeed Wio Tracker L2 (`wio_l2`) |
| `--fake`/`-f` | Use the board's fake-API build (`--m9 --fake` → `m9_fake`, `-w -f` → `wio_l2_fake`) |
| `ENV` | Or name any firmware env directly; with neither, a menu lists every firmware env |
| `--port`/`-p PORT` | Serial port for upload and monitor (default: auto-detect) |
| `--erase`/`-E` | Erase the whole flash in the same esptool session as the upload (`upload_erase` target); wipes saved settings |
| `--fullclean`/`-F` | `pio run -t fullclean` first |
| `--just-build`/`-B` | Compile only, every firmware env or the one named; prints a pass/fail summary |

If a first build fails while installing a tool package, run it again. A
one-off PlatformIO install error has been seen on the first run.
