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

## Time zone data

`src/tz/tz_data.h` holds the zone boundaries and POSIX strings for the
automatic time zone. `scripts/tzgen.py` (Python 3 standard library only)
generates it; the downloaded data stays outside the repo.

1. Download `timezones.geojson.zip` (the no-oceans file, not
   `-with-oceans`) from the
   [timezone-boundary-builder](https://github.com/evansiroky/timezone-boundary-builder/releases)
   release, currently `2026e`, to a folder outside the repo.
2. Regenerate, with tzdata from `/usr/share/zoneinfo` (its `+VERSION` was
   `2026e`):

   ```sh
   python3 -I scripts/tzgen.py /path/to/timezones.geojson.zip --release 2026e > src/tz/tz_data.h
   ```

   It prints `zones 419, rings 1098, points 146728, blob 1158338 bytes` for
   `2026e` at the default `--tolerance 0.01` (degrees). The blob must stay
   under 2 MiB; if it grows past that, the script fails and a larger
   `--tolerance` (`0.015`, then `0.02`) brings it down.
3. If the script lists zones missing from zoneinfo, point `--zoneinfo` at a
   newer tzdata, for example the `zoneinfo` folder of the `tzdata` pip package
   installed in a virtual environment outside the repo.

The unit-test fixture `test/fixtures/tz_fixture_blob.h` comes from the same
script:

```sh
python3 -I scripts/tzgen.py test/fixtures/tz_fixture.geojson \
    --posix test/fixtures/tz_fixture_posix.json --no-simplify \
    --release test --name kTzFixture --namespace fx > test/fixtures/tz_fixture_blob.h
```

The data is © OpenStreetMap contributors under the ODbL; the README's
"Time zone data" section carries the attribution.
