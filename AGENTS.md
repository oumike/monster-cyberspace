# Monster for Cyberspace — agent instructions

Standalone ESP32 firmware client for [cyberspace.online](https://cyberspace.online),
a retro text-only social network. Milestone 1 is cIRC chat plus a device-served
web config page, on the Elecrow ThinkNode M9. The Seeed Wio Tracker L2 and
the LilyGO T-Deck Plus followed, then the Elecrow CrowPanel Advance 3.5" and
5.0", the LilyGO T-LoRa Pager, the LilyGO T-Deck Pro and the Attaky Mesh Deck
(all built, not yet tried on hardware; the Mesh Deck boots), and the Attaky
Cyber Deck Plus (contributed in PR #6; an early version was used on hardware by
its contributor). The Xteink X4
(cIRC rooms only) is built and has booted on hardware; for now it is a
development-only target, left out of releases, CI and the native tests. The M5Stack Cardputer ADV
(8 MB, no PSRAM, no Guilds) is built, not yet tried on hardware, and so is
the LilyGO T-Display P4 (ESP32-P4, AMOLED). The Tanmatsu (ESP32-P4,
keyboard) runs on hardware and is released and in the flasher. More
boards and Cyberspace features come later.

- **Design spec:** `docs/superpowers/specs/2026-10-25-cyberspace-m9-circ-design.md`
- **Milestone 2 (C-Mail):** spec `docs/superpowers/specs/2026-10-25-cyberspace-m9-cmail-design.md`,
  plan `docs/superpowers/plans/2026-10-25-cyberspace-m9-cmail.md`.
- **Guilds:** spec `docs/superpowers/specs/2026-10-30-cyberspace-guilds-design.md`,
  plan `docs/superpowers/plans/2026-10-30-cyberspace-guilds.md`.
- **People (profiles, follow, poke):** spec
  `docs/superpowers/specs/2026-10-31-cyberspace-people-design.md`.
- **Milestone 3 (Feed):** spec `docs/superpowers/specs/2026-10-26-cyberspace-m9-feed-design.md`,
  plan `docs/superpowers/plans/2026-10-26-cyberspace-m9-feed.md`.
- **GPS time:** spec `docs/superpowers/specs/2026-10-26-cyberspace-gps-time-design.md`,
  plan `docs/superpowers/plans/2026-10-26-cyberspace-gps-time.md`.
- **Settings backup:** spec `docs/superpowers/specs/2026-10-26-cyberspace-backup-design.md`,
  plan `docs/superpowers/plans/2026-10-26-cyberspace-backup.md`.
- **Touch UI:** spec `docs/superpowers/specs/2026-10-26-cyberspace-touch-design.md`,
  plan `docs/superpowers/plans/2026-10-26-cyberspace-touch.md`.
- **T-Deck Plus:** spec `docs/superpowers/specs/2026-10-26-cyberspace-tdeck-plus-design.md`,
  plan `docs/superpowers/plans/2026-10-26-cyberspace-tdeck-plus.md`.
- **Mesh Deck:** survey `docs/superpowers/specs/2026-10-29-cyberspace-mesh-deck-design.md`,
  plan `docs/superpowers/plans/2026-10-29-cyberspace-mesh-deck.md`.
- **Xteink X4:** spec `docs/superpowers/specs/2026-10-30-cyberspace-x4-design.md`,
  plan `docs/superpowers/plans/2026-10-30-cyberspace-x4.md`.
- **Attaky Cyber Deck Plus:** spec `docs/superpowers/specs/2026-11-01-cyberspace-cyber-deck-plus-design.md`,
  plan `docs/superpowers/plans/2026-11-01-cyberspace-cyber-deck-plus.md`.
- **M5Stack Cardputer ADV:** spec `docs/superpowers/specs/2026-11-01-cyberspace-cardputer-adv-design.md`,
  plan `docs/superpowers/plans/2026-11-01-cyberspace-cardputer-adv.md`.
- **LilyGO T-Display P4:** spec `docs/superpowers/specs/2026-11-02-cyberspace-tdisplay-p4-design.md`,
  plan `docs/superpowers/plans/2026-11-02-cyberspace-tdisplay-p4.md`.
- **Tanmatsu:** spec `docs/superpowers/specs/2026-11-04-cyberspace-tanmatsu-design.md`,
  plan `docs/superpowers/plans/2026-11-04-cyberspace-tanmatsu.md`.
- **Fluffy Generator (custom mascot):** spec
  `docs/superpowers/specs/2026-10-31-cyberspace-fluffy-generator-design.md`,
  plan `docs/superpowers/plans/2026-10-31-cyberspace-fluffy-generator.md`.
  The presets and default phrases have one source, `site/mascots.json`.
- **User guide:** `docs/USE.md`. **Testing and the live checklist:** `docs/TESTING.md`.
- **Implementation plan:** `docs/superpowers/plans/2026-10-25-cyberspace-m9-circ.md`.
  Work its tasks in order, tick each `- [ ]` box as you finish a step, and
  commit once per task. The plan's "Global Constraints" apply to every task.

## Hard rules

- **Never call the live Cyberspace API** (`api.cyberspace.online` or its
  Firebase RTDB) from tests, scripts, or an AI agent session. The API terms ban
  bots, scraping, and LLM-driven agents. Use the host-test fixtures. A human
  runs the live checklist in `docs/TESTING.md`. Reading the API docs
  (`https://api.cyberspace.online/docs.md`) is allowed: do it whenever API
  behaviour matters, without asking.
- Every write the firmware makes must come from a user's key press. No
  automation, and no automatic retries of sends.
  One exception, on the Xteink X4 only (`board::webCompose()`): text sent
  from the web config page's Send box, one request per send, still no
  retries (`/api/compose`, `ui::webCompose`).
- **No captive portal.** The setup AP (`Monster-Cyberspace-<BOARD>-Setup`,
  without `-Setup` when that passes 32 characters) is open unless a password
  is configured in web config. The page is served at
  `http://192.168.4.1`, with no DNS hijacking and nothing that pops up.
- Never log or store passwords, except that backup files
  (`docs/superpowers/specs/2026-10-26-cyberspace-backup-design.md`) deliberately
  contain the Wi-Fi, page and setup-AP passwords and the refresh token. Never
  log them, or tokens.
- **More boards are planned.** Keep Cyberspace logic board-agnostic: portable
  logic in `lib/core`, and hardware access outside `src/hal/` only through
  `src/hal/board.h`. Never include `src/hal/<board>/` headers elsewhere or
  hard-code panel size, pins or key codes.
- **Long-term idea, not to be built yet:** a LoRa gateway that bridges cIRC to
  other devices. It needs the Cyberspace operator's permission first (spec §12).

## Commands

```sh
pio test -e native          # host unit tests (lib/core)
node --test site/*.test.mjs # flasher site tests (not `node --test site/`)
pio run -e m9               # firmware
pio run -e wio_l2           # Seeed Wio Tracker L2 firmware
pio run -e tdeck            # LilyGO T-Deck Plus firmware
pio run -e crowpanel35      # Elecrow CrowPanel Advance 3.5" (crowpanel50: the 5.0")
pio run -e pager            # LilyGO T-LoRa Pager
pio run -e tdeck_pro        # LilyGO T-Deck Pro (e-paper)
pio run -e mesh_deck        # Attaky Mesh Deck
pio run -e cyber_deck_plus  # Attaky Cyber Deck Plus
pio run -e x4               # Xteink X4 (ESP32-C3, no PSRAM)
pio run -e cardputer_adv    # M5Stack Cardputer ADV (8 MB, no PSRAM, no Guilds)
pio run -e tdisplay_p4      # LilyGO T-Display P4 (ESP32-P4, AMOLED)
pio run -e tanmatsu         # Tanmatsu (ESP32-P4, keyboard)
scripts/build-upload-monitor.sh m9   # flash + serial monitor (115200); -w for the Wio L2, -t for the T-Deck Plus, -3/-5 for the CrowPanels, -P for the Pager, -D for the T-Deck Pro, -M for the Mesh Deck, -K for the Cyber Deck Plus, -X for the X4, -C for the Cardputer ADV, -4 for the T-Display P4, -T for the Tanmatsu; --help for options
scripts/build-upload-monitor.sh -B   # build every firmware env, no device needed
scripts/screenshot.py PORT [--watch]  # PNG screenshots over the serial console (no device writes)
# demo build for screenshots, no network (never release): docs/BUILD.md "Screenshots"
PLATFORMIO_BUILD_DIR=.pio/demo PLATFORMIO_BUILD_FLAGS=-DRC_FAKE_API=1 pio run -e mesh_deck
scripts/release.sh --dist-only       # release images into dist/; releasing itself is the user's call (docs/BUILD.md)
```

## Layout

`lib/core/` holds portable, Arduino-free logic with native tests in `test/`.
`src/` holds networking, tasks, screens, and the web server; each board's HAL
sits behind `src/hal/board.h` in `src/hal/<board>/` (boards return portable
keys from `board::pollKey`). M9 hardware references live in the sibling
projects `../../camillia/camillia-mt/src/hal/hw_m9.h` and `../../rhino-moshi/firmware/`.
`src/hal/wio_l2/` is the Seeed Wio Tracker L2, ported from
`../../camillia/camillia-mt` (`src/hal/hw_wio_tracker_l2.h`, `wio_tracker_l2_io.cpp`)
and `../../platy/platy-aprs/src/hal/` (`pins_wio_l2.h`, `wio_l2_io.cpp`, `audio.cpp`).
`src/hal/tdeck/` is the LilyGO T-Deck Plus (from camillia-mt's `hw_tdeck.h`). It
reuses the M9's `st7789` and the Wio's `gt911` drivers through config structs,
and sets the `hasKeyboard`, `backspaceIsBack` and `anyInputWakes` flags; its
input rules (`keymap_tdeck`, `backspace`, `click_timer`) live in `lib/core`.
`src/hal/crowpanel/` is the Elecrow CrowPanel Advance 3.5" (ILI9488 through
LovyanGFX, SD, optional GPS) and 5.0" (800x480 RGB through ESP-IDF's
`esp_lcd`, rotated in software; backlight and touch reset on a PCA9557 or a
board MCU, probed at boot), from camillia-mt's `hw_crowpanel_35.h`,
`hw_crowpanel_50.h` and `crowpanel_50_display.h`. Both reuse the `gt911`
driver and are touch only, with no buttons, battery sense, buzzer or RTC.
`src/hal/pager/` is the LilyGO T-LoRa Pager (from camillia-mt's pager HAL):
an ST7796 480x222 through the shared `st7789` driver (offsets and an init
list in its `Config`), an XL9555 expander for power rails, a TCA8418
keyboard, the side wheel as Up/Down, the Wio's `es8311` speaker driver,
BQ25896 battery, PCF85063 RTC, GPS and SD. No touch; its input rules
(`keymap_pager`, `rotary`) live in `lib/core`.
`src/hal/tdeck_pro/` is the LilyGO T-Deck Pro (from camillia-mt's
`hw_tdeck_pro.h`/`tdeck_pro_display.h` and platy-aprs's `display_eink.cpp`,
`tdeck_pro_io.cpp`): a GDEQ031T10 240x320 e-paper through GxEPD2, with frames
packed to 1 bit and refresh timing by `ui/eink` in `lib/core`; a TCA8418
keyboard (`keymap_tdeck_pro`), raw CST328/CST3530 touch, the side button as
ScreenOff, BQ27220/BQ25896 battery, GPS and SD. `board::eink()` makes the app
use `paperTheme()` (outlined selections, `Theme::outlined`), hide the Theme
item, and keep the splash cursor and screensaver still.
`src/hal/attaky/` is the Attaky Core both Attaky decks share
(`pins_attaky_core.h`, `attaky_core.{h,cpp}`: panel pins, bus recovery, the
AW9523 front expander and keyboard module, FT6636, MAX17048, LEDs).
`src/hal/mesh_deck/` is the Attaky Mesh Deck 1.0 (from camillia-mt's
`hw_mesh_deck.h` and platy-aprs's `mesh_deck_io.cpp`, `pins_mesh_deck.h`; its
GPS and SX1262 sleep, the rest through `src/hal/attaky/`): an
ST7789 through the shared `st7789` driver (MADCTL 0xA0, panel mounted 180°),
FT6636 touch, the AW9523 front buttons (`MeshDeckPad`) and keyboard module
(`MeshDeckKeys`, both in `input/keymap_mesh_deck`), MAX17048 gauge and GPS; no
RTC, SD or buzzer. The LoRa module's SX1262, when fitted, is unused: `radioSleep()`
puts it to sleep at boot (awake, it whines). Console: UART0 (CH340). Its RGB LEDs are the notification
light (`board::hasNotifyLight`/`notifyLight`; the blinker, colours and
timeouts in `ui/notify_light`, settings `notifyLight`/`notifyTimeoutSecs`).
`src/hal/cyber_deck_plus/` is the Attaky Cyber Deck Plus 1.0 (PR #6, from the
Attaky "cyberdeck plus 1.0" datasheet): the Mesh Deck's Core through
`src/hal/attaky/`, plus a gamepad of two TCA9555s at 0x22/0x26
(`CyberDeckGamepad` in `input/keymap_cyber_deck`: D-pad arrows, A Select, B and
L1 Back, holds as the Core's), Media I/O's NS4168 amplifier on I2S
(IO17/18/21; the channel idles after 1 s of silence) and microSD over 1-bit
SDMMC. No GPS, LoRa or RTC; `wakeHint()` is R2.
`src/hal/x4/` is the Xteink X4 (from rhino-moshi's `include/boards/x4.h`,
`panel_x4.cpp`, `buttons.cpp`, `power.cpp`): an ESP32-C3 with no PSRAM, so
the renderer draws in strips (`ui/renderer`, `sys/mem`) and `RC_CHAT_CAP` /
`RC_MAX_REPLIES` shrink the caches (20 messages: a TLS handshake can take
~55 KB). With `RC_LOW_MEM` the room log keeps no laid-out lines: it indexes
which message each line belongs to and lays out only what is on screen,
drawing each line as it comes (`LogView::beginLines`, `layoutRoomEach`), and
`loadOlder` stops below 68 KB free. A GDEQ0426T82 e-paper driven through the
bare GxEPD2 driver from a 48 KB 1-bit frame (portrait, packed turned to the
native landscape), refreshed by `ui/eink`; buttons on two ADC ladders
(`input/keymap_x4`) sampled by a 5 ms task; battery on GPIO0, latch GPIO13,
SD. `board::canType()` is false (text fields open the canned picker,
`ui/widgets/canned_picker`, with phrases from web config), `webCompose()` is
true (web config's Send box, `/api/compose`, `web/compose_req`), and
`sleepsOnBattery()` drives `app/power_service` (`power/power_policy`).
For TLS heap it is barebones: cIRC rooms only (`CMAIL`, `FEED`, `GUILDS`,
`PEOPLE`, `UNREAD`, `SPELL`, `EMOJI`, `OTA` and `MASCOT` are 0; it is in
`UNRELEASED_ENVS` in `scripts/release.sh`, so releases and the flasher leave it
out until it is ready; CI's build skips it and the native env's `test_ignore`
skips `test_x4_input`), the open room
polled on the one TLS connection (`kPollRoom`), and a `custom_sdkconfig`
(16 KB in / 4 KB out mbedTLS records, dynamic buffers) that rebuilds ESP-IDF,
so a local build needs a project and `core_dir` path without spaces
(docs/BUILD.md). `RC_FEAT_CIRC=0` (no rooms) still exists for a board that
wants the rest without them.
`src/hal/cardputer_adv/` is the M5Stack Cardputer ADV (ESP32-S3FN8, 8 MB, no
PSRAM; `partitions_8mb.csv`: one factory app, `nvs` at 0x7D0000): an ST7789
240x135 through the shared `st7789` driver (MADCTL 0x60, offsets col 40 /
row 53), a TCA8418 keyboard (`CardputerKeys` in `input/keymap_cardputer_adv`:
Fn+; . , / arrows, plain ones outside text fields; ` leaves a text field, else Back; Fn+Backspace Home), the Wio's `es8311` driver
clocked from BCLK (`mclkFromBclk`), battery ADC and SD; no touch, RTC or GPS.
Text is fixed at Compact 6x12 (`board::fixedTextSize`; Text size hidden in
Settings and web config). Feature flags in `lib/core/src/feature_flags.h`
(`RC_FEAT_CIRC`/`CMAIL`/`FEED`/`GUILDS`/`PEOPLE`/`UNREAD`/`SPELL`/`EMOJI`/`OTA`/`MASCOT`,
all 1 by default; `RC_FEAT_ENTRIES`/`COMPOSER` follow): there `GUILDS`,
`SPELL`, `EMOJI`, `OTA` and `MASCOT` are 0, so it has cIRC, C-Mail, the Feed,
People and Unread, and a wide emoji draws `?`. Unread's Home count is
`countUnread()` (no copies, it runs every frame) and its screen rebuilds its
rows only when the count changes or once a second. `RC_MONO_CELLS` (default `RC_LOW_MEM`) is 0 so
it keeps colour; `src/sys/string_instances.cpp` keeps Matter/BLE out of the
link. `NO_OTA_ENVS` in `scripts/release.sh` ships it a factory image only,
and the flasher hides Make your Fluffy and Phrases for it.
`src/hal/tdisplay_p4/` is the LilyGO T-Display P4 with the 4.1" AMOLED (ESP32-P4,
`esp32p4_es`, 32 MB PSRAM; ported from `../../wadamesh` `variants/tdisplay_p4/`,
pins as camillia-mt's `hw_tdisplay_p4.h`): an XL9535 expander at 0x20 on Wire
7/8 (`io_xl9535`; rails, resets, GPS wake bit 9, C6 enable, SD power), the
RM69A10 over MIPI-DSI (`esp_lcd_rm69a10`, `panel_rm69a10`) whose native
568x1232 frame the UI fills at 2x as a 284x616 panel (`ui/scale2x`), GT9895
touch, the clip-on keyboard expansion (`keyboard_tdisplay_p4`: XL9555 +
TCA8418 on Wire1 46/45, found once in `board::begin()`; `TdisplayP4Keys` in
`input/keymap_tdisplay_p4`, F11 steps the light, `Action::KbLightStep`,
`nextKbLight`), the ES8311 speaker through the Wio's `es8311` driver (44.1 kHz,
`wire`/`tryLock` config) and the AW86224 haptic driver (`haptic_aw86224`, on
its own task) taking turns on Wire1 20/21 (`i2c1_tdisplay_p4`), GPS, PCF8563
RTC, BQ27220 gauge and 4-bit SD mounted with ESP-IDF's
`esp_vfs_fat_sdmmc_mount` (Arduino `SD_MMC` drives GPIO45, the keyboard's SCL)
in `peripherals_tdisplay_p4.cpp`. Rotation 1, portrait, is its first-boot
default (`board::defaultRotation()`); with the keyboard fitted it locks to
rotation 2 (landscape above the keys) and leaves the saved one alone. The UI
is drawn `CORNER_INSET` (16 px) in from every edge, clear of the rounded corners.
`board::hasHaptic`/`haptic` drive tap (`ui/haptics` `tapBuzz`, 15 ms) and alert
(120 ms) buzzes, switched by settings `tapVibrate`/`alertVibrate`. Wi-Fi goes
through the on-board ESP32-C6 running ESP-Hosted 2.12.13 (docs/BUILD.md);
P4 OTA never updates the C6. Its ROM loads the bootloader from 0x2000
(`env_boot_offset` in `scripts/release.sh`).
`src/hal/tanmatsu/` is the Nicolai Electronics Tanmatsu (ESP32-P4 `esp32p4_es`,
32 MB PSRAM; from wadamesh's `tanmatsu/` and its badge-bsp, tanmatsu_coprocessor,
mipi_dsi_abstraction and es8156 components). Standalone: the image replaces the
Tanmatsu launcher. A CH32V203 coprocessor at 0x5F on Wire 9/10
(`coproc_tanmatsu`, one mutex for every I2C user) owns the keyboard (9-byte
bitfield, `TanmatsuKeys` in `input/keymap_tanmatsu`: held modifiers, AltGr
layer, F1–F6 and Fn+F1/F4/F5, Meta tapped for Home), display and keyboard
backlights, six RGB LEDs (LED 2 the notification light, LED 0 lit dim green
since manual mode stops the power LED), the power button (unused; volume down is ScreenOff), the C6's
power, battery and RTC. The ST7701 480x800 over MIPI-DSI (`panel_st7701`) is
drawn at 2x as a 400x240 landscape panel (`ui/scale2x`, `PANEL_ROTATION`);
no touch, no rotation. Wi-Fi is the C6's own esp-hosted-tanmatsu image,
restarted through the coprocessor before `hostedSetPins`. ES8156 speaker
(`audio_es8156`, own task, amplifier on only around a beep), SD_MMC 4-bit,
no GPS. `board::functionKeys()` gives the F10 help its own table. Released
and in the flasher since 2026-11-04.
Touch: `lib/core` turns samples into gestures and resolves them against the
per-frame `TapMap` widgets fill as they draw (`input/gestures`, `ui/tap_map`,
`input/touch_resolve`, the `Osk` keyboard); `ui_task` feeds `handleKey` and
docks the keyboard. Boards without touch register nothing.

## Progress (hand-off from Claude Code, 2026-10-25)

Work is on **`main`** at https://github.com/oumike/monster-cyberspace. All 22 tasks of the M1 plan, all 6 of the
M2 (C-Mail) plan and all 12 of the M3 (Feed) plan are done and committed, with
their boxes ticked. The room-screen, C-Mail, web config and Feed (`m9_fake`)
hardware checks passed on the device (2026-10-26).
Also done since: themes (Default / Black & white / White & black plus
cyberspace.online's twelve, Dark … Top8, then our Bubblegum Dark, Top8 Dark, Sepia Dark and C64 Dark, from the site's colours; picked in a
Settings box and web config; saved by index, so append new ones), and the Wio Tracker L2 HAL with env `wio_l2`
(bring-up only, untested on hardware).
All 8 tasks of the GPS time plan are done and committed: NTP, GPS and RTC
clock sources (`src/app/time_service.cpp`), the GPS powering itself off,
the offline automatic time zone, the NTP server field, and the gps, time and
zone lines in Settings and web config (built without flashing).
All 8 tasks of the backup plan are done and committed: backup YAML in
`lib/core/src/config/`, SD access in the HAL (mounted per operation, the card
is removable), `src/store/backup_store.cpp`, Settings/Setup items and the web
Backup section (built without flashing).
All 12 tasks of the touch plan are done and committed: gestures, tap map,
`Overlay` menus, touch status bar and Home tiles, the docked on-screen
keyboard and four orientations (built without flashing).
All 7 tasks of the T-Deck Plus plan are done and committed: shared panel and
touch drivers, keyboard, Backspace-as-Back, any-input wake, trackball, GPS,
battery, speaker and SD, with env `tdeck` (built without flashing;
`PANEL_MADCTL` and `TOUCH_ROT` await the hardware check).
The CrowPanel Advance 3.5" and 5.0" HALs (envs `crowpanel35`, `crowpanel50`)
were added on 2026-10-27 (built without flashing; the 5.0"'s portrait
direction and both touch mappings await the hardware check).
The scripted fake-API builds (`*_fake` envs) were removed on 2026-10-27 at
the user's request; every firmware env talks to the real API. The fake
sources came back on 2026-10-29 as an unreleased demo build for screenshots
(`RC_FAKE_API` through `PLATFORMIO_BUILD_FLAGS`, no env), driven with the
console's `key`/`type` commands.
The LilyGO T-LoRa Pager HAL (env `pager`) was added on 2026-10-27 (built
without flashing; `PANEL_MADCTL` and the row offset await the hardware
check; the wheel direction was flipped after a hardware check on 2026-10-28).
The LilyGO T-Deck Pro HAL (env `tdeck_pro`) was added on 2026-10-28 (built
without flashing).
The Attaky Mesh Deck HAL (env `mesh_deck`) and the notification light were
added on 2026-10-29 (flashed: it boots and finds touch, gauge and keyboard;
the panel, touch mapping, keys and LEDs await the hardware check).
**Still open:** the unticked items under "Open hardware checks" in
`docs/TESTING.md` (themes, device Settings, touch UI, Wio L2, T-Deck Plus, CrowPanels, Pager, T-Deck Pro, Mesh Deck, Cyber Deck Plus, Cardputer ADV, T-Display P4, Tanmatsu, GPS time, API
assumptions, settings backup, OTA, Fluffy Generator), then the human live checklist on the `m9` build.
CI (`.github/workflows/build.yml`) runs on GitHub.
OTA updates were added on 2026-10-28 (built without flashing): signed images
straight from GitHub releases over HTTPS (`src/app/ota_service.cpp`,
`lib/core/src/ota/`), and `partitions_16mb.csv` moves NVS past the app slots
so installs keep settings (`src/store/nvs_migrate.cpp`; docs/BUILD.md).
The T-Deck Pro (env `tdeck_pro`, e-paper) uses the touch-only paradigm plus a
▲ ● ▼ side strip on list screens (`board::sideNav()`, `ui/widgets/side_nav`);
its and the T-Deck Plus's mic key opens the emoji tray (`board::textEntry`).
A browser flasher (GitHub Pages, `site/`, `.github/workflows/pages.yml`)
serves the latest release (docs/BUILD.md).
Guilds were added on 2026-10-30 (built without flashing): browse, threads,
info, members, join, leave, make my guild, new thread and alerts, opened by
the M9's Map button or Home.
People were added on 2026-10-31 (not built yet): profiles, follow, unfollow,
poke and a user's entries, opened from rooms, conversations, entries and
guild members (`lib/core/src/people/`, `src/app/people_worker.cpp`,
`src/screens/profile.cpp`). Blocking is website-only (no API call).
My profile was added on 2026-11-01 (branch `my-profile`, its own PR and
release): a Home **Profile** tile and row (with People) opening your own
profile, whose Entries lists your posts; with eight tiles Unread no longer
spans the top row (`ui/home_items` `homeSpanFirst`).
Unread (issue #3) was added on 2026-10-31 (not built yet): one screen of unread
C-Mail, mentions and Feed notices, newest first (`ui/unread_inbox`,
`src/screens/notifications.cpp`, `router::goFrom` for Back), from Home on every board.
The Attaky Cyber Deck Plus (env `cyber_deck_plus`) came in PR #6 on
2026-11-01 with three app changes for every board: a pinned "+ New entry" row
first in the Feed, the connection state reaching live with no room open (and
staying offline while signed out), and Tab opening the entry menu; plus
Friends (device-only; removed on 2026-11-03 to stay in line with
cyberspace.online). Home's Following item lists Feed entries by people you
follow (`FeedListScreen::Kind::Following`, `CmdType::OpenFollowingPosts`): the
API can't list them or look users up by id, so `PeopleWorker` scans Feed pages
for followed `authorId`s. The maintainer review moved the Core code both
Attaky decks share into `src/hal/attaky/`; the Mesh Deck and Cyber Deck Plus
hardware checks are open (docs/TESTING.md).
The Fluffy Generator was added on 2026-10-31 (built without flashing; the
T-Deck Plus hardware check and the flasher browser check are open): custom
mascot art and phrases in a 4 KB `mascot` partition at 0xcb0000
(`lib/core/src/mascot/`, `src/store/mascot_store.cpp`), set from the
flasher's Make your Fluffy and Phrases modals or web config's Mascot section.
The M5Stack Cardputer ADV (env `cardputer_adv`) was added on 2026-11-01
(built without flashing; RAM 61,368 B, flash 1,953,452 B): compile-time
feature flags (`lib/core/src/feature_flags.h`) cut it to cIRC rooms, with no OTA
or mascot partition. Its panel offsets, ES8311 clock, push speed, key map and
TLS heap headroom await the hardware check (docs/TESTING.md).
The LilyGO T-Display P4 (env `tdisplay_p4`) was added on 2026-11-02 (built
without flashing): portrait, landscape, or landscape with the keyboard
expansion; tap and alert vibration settings (`tapVibrate`/`alertVibrate`, shown
only where `board::hasHaptic()`). Its panel, touch axes, keyboard, haptics,
speaker, C6 Wi-Fi, GPS, RTC, gauge and SD await the hardware check
(docs/TESTING.md). Also on 2026-11-02: the T-Deck Pro draws 6 px in from the
left (`PANEL_LEFT_INSET` in `pins_tdeck_pro.h`, `packMono` x0) so the bezel
doesn't cover text; its check is open too.
The Tanmatsu (env `tanmatsu`) was added on 2026-11-04 (built without
flashing): landscape at 2x with its keyboard. It runs on hardware; brightness,
LED and status-light settings followed, and it joined releases and the
flasher the same day. The rest of its hardware check is open (docs/TESTING.md).
The native suite is green: 743/743
(`pio test -e native`), and `scripts/build-upload-monitor.sh -B` builds all
thirteen firmware envs (the X4 only from a path without spaces).

Notes for whoever continues:
- Each task follows test-driven development: write the plan's tests first, run
  them and watch them fail, implement, run them green, then commit with the
  plan's commit message.
- OTA signing: `ota_signing_key.pem` is git-ignored and must never be
  committed, logged or changed; `src/app/ota_signing_pubkey.h` must stay its
  public half. Each HAL's `board::otaSlug()` must match the release file name.
  The update check talks to github.com only, never the Cyberspace API.
- Cell fonts: `src/ui/fonts/cell*.h` are generated by `scripts/bdf2cells.py`
  from Terminus 4.49.1, every glyph it has (download outside the repo, run
  with `python3 -I`). UI glyphs Terminus lacks are aliased in
  `lib/core/src/ui/glyph_set.h`.
- Emoji: every Noto Emoji code point Terminus lacks (`lib/core/src/ui/emoji_wide.h`)
  takes two cells; the right one holds a private-use tail code point
  (`ui/text_width.h`). `emoji_wide.h` and `src/ui/fonts/emoji_cells.h` are
  generated by `scripts/emoji2cells.py` from Noto Emoji and a Terminus BDF
  (needs Pillow and fontTools; download the fonts outside the repo). The
  tray's 40 are in `ui/emoji_set.h`.
- Spell check (from camillia-mt): the engine is `lib/core/src/spell/`, the
  strip `ui/widgets/spell_strip.h`. `tools/gen_spell_dict.py` (a `pre:` script
  in every firmware env) builds the git-ignored `src/spell_dict_en.h` from
  `dict/en.txt` (CC-BY-SA-4.0; rebuild it with `tools/build_spell_wordlist.py`).
- Time zone (user decision): manual only, no GPS auto-zone. The curated list
  is `lib/core/src/tz/zones.cpp`; picked on the setup page, in web config and
  in device Settings.
- Wake (user decision): holding centre wakes the screen. A centre tap while
  dark shows a 10 s read-only screensaver (`src/app/ui_task.cpp`, spec §5).
- C-Mail reuses `RoomScreen` (`RoomScreen{true}` is `Id::Conversation`) and the
  net worker's room channel (`dm_`: `dm_messages`, plus `dm_presence` for typing).
  Typing writes come only from key presses (`RoomScreen::noteTyping`).
- Setup mode (user decision): there is no boot gesture. It runs at boot with
  no Wi-Fi saved, from Home → Settings → "Turn setup AP on", or by itself
  after the saved networks fail for 5 minutes (`rc::ApFallback`; that AP drops
  once Wi-Fi is back and nobody is joined). Factory reset lives in Home →
  Settings and in web config. The M9 HAL no longer reads BOOT (GPIO0).
- If the first `pio run` fails with
  `FileNotFoundError: 'package-postinstall.py'` while installing
  `esptoolpy`, run it again. It's a one-off PlatformIO install glitch.
- The design mockups the user approved are described in spec §5–§6. The
  original HTML lives in the git-ignored `.superpowers/brainstorm/` folder.
