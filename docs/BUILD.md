# Building

## Prerequisites

- [PlatformIO Core](https://platformio.org/install/cli) 6.1 or newer. The first
  build downloads the pioarduino platform (Arduino core 3.3 / ESP-IDF 5.5).
- A USB data cable. The ThinkNode M9's console runs through its USB-to-UART
  bridge at 115200 baud.

## Environments

| Env | What it builds |
|---|---|
| `m9` | The firmware for the Elecrow ThinkNode M9 |
| `m9_fake` | The same firmware with a scripted fake Cyberspace API, for trying every screen without an account |
| `native` | Host unit tests for `lib/core` |

```sh
pio test -e native                    # unit tests on your computer
pio run -e m9                         # build
scripts/build-upload-monitor.sh m9    # build, flash, then open the serial monitor
```

`scripts/build-upload-monitor.sh` options (`--help` lists them):

| Option | What it does |
|---|---|
| `ENV`, `--m9`/`-9`, `--m9-fake`/`-f` | Pick the firmware; with none, a menu lists every firmware env |
| `--port`/`-p PORT` | Serial port for upload and monitor (default: auto-detect) |
| `--erase`/`-E` | Erase the whole flash in the same esptool session as the upload (`upload_erase` target); wipes saved settings |
| `--fullclean`/`-F` | `pio run -t fullclean` first |
| `--just-build`/`-B` | Compile only, every firmware env or the one named; prints a pass/fail summary |

If a first build fails while installing a tool package, run it again. A
one-off PlatformIO install error has been seen on the first run.
