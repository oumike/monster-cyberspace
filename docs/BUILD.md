# Building

## Prerequisites

- [PlatformIO Core](https://platformio.org/install/cli) 6.1 or newer. The first
  build downloads the pioarduino platform (Arduino core 3.3 / ESP-IDF 5.5)
  into `.pio-core/`, kept apart from the shared `~/.platformio`.
- A USB data cable. The ThinkNode M9's console runs through its USB-to-UART
  bridge at 115200 baud, and so do the CrowPanels', the Mesh Deck's and the Cyber Deck Plus's (a CH340); the Wio Tracker
  L2's, the T-Deck Plus's and the Cardputer ADV's run through the ESP32-S3's
  own USB, the Xteink X4's through the ESP32-C3's, and the T-Display P4's
  through the ESP32-P4's.

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
| `cyber_deck_plus` | The firmware for the Attaky Cyber Deck Plus (the Mesh Deck's Core plus a gamepad, Media I/O speaker and microSD; no GPS or LoRa; an early version was used on hardware by its contributor) |
| `x4` | The firmware for the Xteink X4 (ESP32-C3, no PSRAM, e-paper, buttons only; cIRC rooms only, text sent with phrases or from web config; no OTA; boots on hardware. Rebuilds ESP-IDF for smaller TLS buffers, so build it from a path without spaces) |
| `cardputer_adv` | The firmware for the M5Stack Cardputer ADV (ESP32-S3FN8: 8 MB flash, no PSRAM; keyboard, 240x135 panel; no Guilds, no OTA; not yet tried on hardware) |
| `tdisplay_p4` | The firmware for the LilyGO T-Display P4 (ESP32-P4, 4.1" AMOLED drawn at 2x, touch, optional keyboard expansion, haptics; Wi-Fi through its ESP32-C6; not yet tried on hardware) |
| `tanmatsu` | The firmware for the Tanmatsu (ESP32-P4, 480x800 panel drawn at 2x in landscape, keyboard; Wi-Fi through its ESP32-C6). Replaces the Tanmatsu launcher. Development only: not released or in the flasher until it has been tried on hardware |
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
pio run -e cyber_deck_plus            # build for the Cyber Deck Plus
pio run -e x4                         # build for the Xteink X4
pio run -e cardputer_adv              # build for the Cardputer ADV
pio run -e tdisplay_p4                # build for the T-Display P4
pio run -e tanmatsu                   # build for the Tanmatsu
scripts/build-upload-monitor.sh m9    # build, flash, then open the serial monitor
```

The `x4` env sets `custom_sdkconfig`, so pioarduino recompiles ESP-IDF for it
(several minutes the first time). ESP-IDF refuses a path with spaces, and a
symlink doesn't help because it resolves the real path. On a drive like
`/Volumes/T7 Shield`, copy the project somewhere without spaces and point
`core_dir` in that copy's `platformio.ini` at a folder without spaces too.
If the first build stops with `IncompatiblePlatform` after the ESP-IDF
compile, run it again.

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
| `--cyber-deck-plus`/`-K` | Board flag: Attaky Cyber Deck Plus (`cyber_deck_plus`) |
| `--x4`/`-X` | Board flag: Xteink X4 (`x4`) |
| `--cardputer-adv`/`-C` | Board flag: M5Stack Cardputer ADV (`cardputer_adv`) |
| `--tdisplay-p4`/`-4` | Board flag: LilyGO T-Display P4 (`tdisplay_p4`) |
| `--tanmatsu`/`-T` | Board flag: Tanmatsu (`tanmatsu`) |
| `ENV` | Or name any firmware env directly; with neither, a menu lists every firmware env |
| `--port`/`-p PORT` | Serial port for upload and monitor (default: auto-detect) |
| `--erase`/`-E` | Erase the whole flash in the same esptool session as the upload (`upload_erase` target); wipes saved settings |
| `--fullclean`/`-F` | `pio run -t fullclean` first |
| `--just-build`/`-B` | Compile only, every firmware env or the one named; prints a pass/fail summary |

If a first build fails while installing a tool package, run it again. A
one-off PlatformIO install error has been seen on the first run.

### Feature flags

`lib/core/src/feature_flags.h` holds compile-time flags, each 1 unless an env sets
`-DRC_FEAT_X=0`: `RC_FEAT_CMAIL`, `RC_FEAT_FEED`, `RC_FEAT_GUILDS`,
`RC_FEAT_PEOPLE`, `RC_FEAT_UNREAD`, `RC_FEAT_SPELL`, `RC_FEAT_EMOJI` (off, a
wide emoji draws `?` in its two cells), `RC_FEAT_OTA` and `RC_FEAT_MASCOT`.
`RC_FEAT_ENTRIES` and `RC_FEAT_COMPOSER` follow from them. A feature's own
`.cpp` files wrap their body in `#if RC_FEAT_X`; Home, Settings, web config
and backup restore leave out what is off. Only `cardputer_adv` turns any off
(Guilds, Unread, spell, emoji, OTA and mascot). `RC_LOW_MEM` shrinks caches for boards without
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
`scripts/release.sh` and run `tools/set_app_version.py`; CI checks both. An env
that builds but isn't ready to ship (the X4 for now) goes in `UNRELEASED_ENVS`
instead, which keeps it out of releases and the flasher. The X4 is also left
out of CI's build and, through `test_ignore` in `[env:native]`, its native
test; build it locally with `pio run -e x4`. Its
`board::otaSlug()` must match the release's file name (`env_out_name`: the env
name with `_` turned into `-`), or its devices find no update.

The browser flasher (<https://oumike.github.io/monster-cyberspace/>) is
`site/index.html` (ESP Web Tools). `.github/workflows/pages.yml` publishes it
to GitHub Pages after each release: `scripts/pages-site.sh` copies the latest
release's factory images into the site (browsers can't fetch release assets
directly) and writes a manifest per board. A new board needs a name in the
page's `LABELS`, and one in `src/boards.js` of
[monster-cyberspace-web](https://github.com/oumike/monster-cyberspace-web)
(<https://monster.sumat.org/>). Pages must be set to deploy from GitHub Actions. Boards
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

Every board is an ESP32-S3 but the X4 (ESP32-C3) and the T-Display P4
(ESP32-P4), whose ROM loads the bootloader from `0x2000` instead of `0x0`
(`env_chip` and `env_boot_offset` in `scripts/release.sh`). Its factory image
is still flashed at `0x0` (the merge pads the gap), `scripts/pages-site.sh`
gives its manifest the `ESP32-P4` chip family, and `flash.sh` lets esptool
detect the chip.

### T-Display P4: the ESP32-C6 Wi-Fi image

The T-Display P4 has no radio of its own: Arduino `WiFi` goes over SDIO to
the on-board ESP32-C6, which runs ESP-Hosted from its own 4 MB flash. The C6
must run the ESP-Hosted **2.12.13** image, the version of the host libraries in
this project's platform (pioarduino 55.03.312, Arduino 3.3.12). That image
ships with the platform, at
`.pio-core/packages/framework-arduinoespressif32-libs/hosted/esp32c6-v2.12.13.bin`
after a first build, and is the one camillia-mt releases as
`camillia-mt-tdisplay-p4-c6-esp-hosted-v2.12.13.bin`.

Neither our factory image nor OTA touches the C6: a P4 update leaves it as it
was. Boards that already run a 2.12.13 C6 image (LilyGO's ESP-Hosted
examples, camillia-mt, wadamesh) need nothing more. Otherwise provision it once
through the board's **3.3 V UART** connector for the C6:

1. Power the T-Display P4 and connect a 3.3 V USB-to-UART adapter: adapter
   TX to board RX, adapter RX to board TX, GND to GND. Never use 5 V logic.
2. Hold the coprocessor's BOOT button, tap its RESET button, then release BOOT.
3. Write the image with `scripts/flash-tdisplay-p4-c6.sh` (from camillia-mt;
   it needs esptool, which `.pio-core` has after a first build, and finds the
   image there when you leave the path off):

   ```sh
   scripts/flash-tdisplay-p4-c6.sh <c6-uart-port> \
     .pio-core/packages/framework-arduinoespressif32-libs/hosted/esp32c6-v2.12.13.bin
   ```

4. Tap the coprocessor's RESET button and restart the P4.

The file is an ESP-Hosted application image, not a merged one: no bootloader
or partition table. The script reads the C6's partition table, writes the
image into its app partition (factory, or `ota_0`) and blanks `otadata`; it
stops without writing if there is no valid table or the image doesn't fit.
Never write it at `0x0` by hand, and don't use LilyGO's ESP-AT image: the
firmware uses Arduino's transparent `WiFi` over ESP-Hosted. The C6's enable
line is on the XL9535 expander, so a wedged C6 needs a board restart.

### Tanmatsu: standalone, not released

The `tanmatsu` image is a whole firmware, like every other board's: flashing
it (`scripts/build-upload-monitor.sh -T`, over the USB-C port's USB-Serial-JTAG)
replaces the Tanmatsu launcher and its apps. PlatformIO writes the bootloader
at `0x2000` itself. The launcher takes over the USB-C port (its BadgeLink
interface, and the ESP32-C6 can show up instead: esptool then stops with
"This chip is ESP32-C6"; the script then tries the next USB serial port, the
given `--port` first, and fails only when every port does), so the first time, start the ESP32-P4 in its
bootloader: hold the power button until it turns off, then hold the **−**
button (third on the right side) while plugging in the USB cable; the screen
stays black. After that the firmware keeps the P4's own USB port. To go back, reinstall the launcher from Nicolai
Electronics' web installer (docs.tanmatsu.cloud). The ESP32-C6 is left alone:
it keeps the esp-hosted-tanmatsu image it ships with, and whether Arduino's
ESP-Hosted host talks to it is the first hardware check (docs/TESTING.md).
The Tanmatsu is in `UNRELEASED_ENVS` and has no flasher entry until then.

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
