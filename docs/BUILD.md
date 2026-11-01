# Building

## Prerequisites

- [PlatformIO Core](https://platformio.org/install/cli) 6.1 or newer. The first
  build downloads the pioarduino platform (Arduino core 3.3 / ESP-IDF 5.5)
  into `.pio-core/`, kept apart from the shared `~/.platformio`.
- A USB data cable. The ThinkNode M9's console runs through its USB-to-UART
  bridge at 115200 baud, and so do the CrowPanels' and the Mesh Deck's (a CH340); the Wio Tracker
  L2's, the T-Deck Plus's and the Cardputer ADV's run through the ESP32-S3's
  own USB, and the Xteink X4's through the ESP32-C3's.

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
| `x4` | The firmware for the Xteink X4 (ESP32-C3, no PSRAM, e-paper, buttons only; a mostly passive client with text sent from web config; not yet built or tried on hardware) |
| `cardputer_adv` | The firmware for the M5Stack Cardputer ADV (ESP32-S3FN8: 8 MB flash, no PSRAM; keyboard, 240x135 panel; cIRC rooms only, no OTA; not yet tried on hardware) |
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
pio run -e x4                         # build for the Xteink X4
pio run -e cardputer_adv              # build for the Cardputer ADV
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
| `--x4`/`-X` | Board flag: Xteink X4 (`x4`) |
| `--cardputer-adv`/`-C` | Board flag: M5Stack Cardputer ADV (`cardputer_adv`) |
| `ENV` | Or name any firmware env directly; with neither, a menu lists every firmware env |
| `--port`/`-p PORT` | Serial port for upload and monitor (default: auto-detect) |
| `--erase`/`-E` | Erase the whole flash in the same esptool session as the upload (`upload_erase` target); wipes saved settings |
| `--fullclean`/`-F` | `pio run -t fullclean` first |
| `--just-build`/`-B` | Compile only, every firmware env or the one named; prints a pass/fail summary |

If a first build fails while installing a tool package, run it again. A
one-off PlatformIO install error has been seen on the first run.

### Feature flags

`lib/core/src/features.h` holds compile-time flags, each 1 unless an env sets
`-DRC_FEAT_X=0`: `RC_FEAT_CMAIL`, `RC_FEAT_FEED`, `RC_FEAT_GUILDS`,
`RC_FEAT_PEOPLE`, `RC_FEAT_UNREAD`, `RC_FEAT_SPELL`, `RC_FEAT_EMOJI` (off, a
wide emoji draws `?` in its two cells), `RC_FEAT_OTA` and `RC_FEAT_MASCOT`.
`RC_FEAT_ENTRIES` and `RC_FEAT_COMPOSER` follow from them. A feature's own
`.cpp` files wrap their body in `#if RC_FEAT_X`; Home, Settings, web config
and backup restore leave out what is off. Only `cardputer_adv` turns any off
(all nine: cIRC rooms only). `RC_LOW_MEM` shrinks caches for boards without
PSRAM (the X4, the Cardputer); `RC_MONO_CELLS` (defaults to `RC_LOW_MEM`)
stores 1-bit cells, so the Cardputer sets it to 0 to keep colour.
`src/sys/string_instances.cpp` keeps Matter and BLE out of the link.

## Screenshots

Every firmware answers `screenshot` on its serial console with the frame on
screen. `scripts/screenshot.py PORT` saves it as a PNG in `screenshots/`;
`--watch` saves each new screen once it settles, while you move around the
device (Ctrl-C stops). It needs pyserial (PlatformIO's Python has it) and opens
the port without resetting the board. Only the frame buffer is read.

The README's screenshots come from the demo build: `RC_FAKE_API` swaps the
network for scripted rooms, C-Mail and Feed with made-up users, a fixed clock,
and no Wi-Fi, so it never touches the Cyberspace API. It also takes
`key <name>` (up, down, left, right, select, back, home, palette, options,
backspace, symbols, emoji, circ, cmail, feed, screenoff) and `type <text>` on
the console, so screens can be driven from a script. Never release it:

```sh
PLATFORMIO_BUILD_DIR=.pio/demo PLATFORMIO_BUILD_FLAGS=-DRC_FAKE_API=1 pio run -e mesh_deck -t upload
scripts/screenshot.py /dev/cu.wchusbserial10 --scale 2 --name home
```

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
signature) and `.elf` (symbols), plus `flash.sh`, `BUILD-INFO.txt` and `SHA256SUMS`.
Envs in `NO_OTA_ENVS` (`cardputer_adv`, built with `RC_FEAT_OTA=0`) get the
factory image and `.elf` only; `--check-targets` checks they set the flag. A dist-only build
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
page's `LABELS`. Pages must be set to deploy from GitHub Actions. Boards
without the mascot partition (`NO_MASCOT_BOARDS` in `site/mascot.mjs`) hide
Make your Fluffy and Phrases and flash the factory image alone; boards without
OTA (`NO_OTA` in `site/index.html`) say updates come from the page.

### Flash layout

Every env but `cardputer_adv` uses `partitions_16mb.csv`: two 6.25 MB app slots at `0x10000` and
`0x650000`, and the settings (`nvs`) at `0xC90000`, past anything a factory
image writes. `0x9000` is `nvs_old`, where the stock `default_16MB.csv` kept
them; `src/store/nvs_migrate.cpp` moves anything there into `nvs` at boot and
erases it. The custom mascot (`mascot`, 4 KB) sits at `0xCB0000`, right after
`nvs`; OTA and factory reset leave it alone, and a board whose table predates
it is read and written at that address raw. Keep the app slots where they
are: the OTA code refuses to update a board whose layout differs
(`layoutSupportsUpdate`).

The Cardputer ADV (8 MB) uses `partitions_8mb.csv`: one `factory` app at
`0x10000` (size `0x7C0000`), `nvs` at `0x7D0000` (`0x20000`) and `coredump` at
`0x7F0000`. No `otadata`, second slot or `mascot`; settings sit past the app,
so a factory image at `0x0` keeps them, and `nvs_migrate` finds no `nvs_old`.

### Mascot

The mascot blob (magic `FLUF`, CRC32; format in the Fluffy Generator spec §3)
is encoded by `lib/core/src/mascot/` on the device and `site/mascot.mjs` in
the browser; both are checked against `test/fixtures/mascot_golden.h`.
When a user changes the art or phrases, the flasher's manifest gets a second
part: the 4 KB blob at `0xCB0000`. With the defaults it uses the normal
one-part manifest and writes nothing there.

The presets and default phrases have one source, `site/mascots.json`.
`tools/gen_mascot_presets.py`, a `pre:` script in every env, generates the
git-ignored `src/web/mascot_presets.h` from it for web config. Run the site
tests with `node --test site/*.test.mjs` (CI does too).

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
