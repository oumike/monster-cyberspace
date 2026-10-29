# Monster for Cyberspace — agent instructions

Standalone ESP32 firmware client for [cyberspace.online](https://cyberspace.online),
a retro text-only social network. Milestone 1 is cIRC chat plus a device-served
web config page, on the Elecrow ThinkNode M9. The Seeed Wio Tracker L2 and
the LilyGO T-Deck Plus followed, then the Elecrow CrowPanel Advance 3.5" and
5.0", the LilyGO T-LoRa Pager, the LilyGO T-Deck Pro and the Attaky Mesh Deck
(all built, not yet tried on hardware; the Mesh Deck boots). More
boards and Cyberspace features come later.

- **Design spec:** `docs/superpowers/specs/2026-10-25-cyberspace-m9-circ-design.md`
- **Milestone 2 (C-Mail):** spec `docs/superpowers/specs/2026-10-25-cyberspace-m9-cmail-design.md`,
  plan `docs/superpowers/plans/2026-10-25-cyberspace-m9-cmail.md`.
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
- **User guide:** `docs/USE.md`. **Testing and the live checklist:** `docs/TESTING.md`.
- **Implementation plan:** `docs/superpowers/plans/2026-10-25-cyberspace-m9-circ.md`.
  Work its tasks in order, tick each `- [ ]` box as you finish a step, and
  commit once per task. The plan's "Global Constraints" apply to every task.

## Hard rules

- **Never call the live Cyberspace API** (`api.cyberspace.online` or its
  Firebase RTDB) from tests, scripts, or an AI agent session. The API terms ban
  bots, scraping, and LLM-driven agents. Use the host-test fixtures. A human
  runs the live checklist in `docs/TESTING.md`.
- Every write the firmware makes must come from a user's key press. No
  automation, and no automatic retries of sends.
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
pio run -e m9               # firmware
pio run -e wio_l2           # Seeed Wio Tracker L2 firmware
pio run -e tdeck            # LilyGO T-Deck Plus firmware
pio run -e crowpanel35      # Elecrow CrowPanel Advance 3.5" (crowpanel50: the 5.0")
pio run -e pager            # LilyGO T-LoRa Pager
pio run -e tdeck_pro        # LilyGO T-Deck Pro (e-paper)
pio run -e mesh_deck        # Attaky Mesh Deck
scripts/build-upload-monitor.sh m9   # flash + serial monitor (115200); -w for the Wio L2, -t for the T-Deck Plus, -3/-5 for the CrowPanels, -P for the Pager, -D for the T-Deck Pro, -M for the Mesh Deck; --help for options
scripts/build-upload-monitor.sh -B   # build every firmware env, no device needed
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
`src/hal/mesh_deck/` is the Attaky Mesh Deck 1.0 (from camillia-mt's
`hw_mesh_deck.h` and platy-aprs's `mesh_deck_io.cpp`, `pins_mesh_deck.h`): an
ST7789 through the shared `st7789` driver (MADCTL 0xA0, panel mounted 180°),
FT6636 touch, the AW9523 front buttons (`MeshDeckPad`) and keyboard module
(`MeshDeckKeys`, both in `input/keymap_mesh_deck`), MAX17048 gauge and GPS; no
RTC, SD or buzzer. Console: UART0 (CH340). Its RGB LEDs are the notification
light (`board::hasNotifyLight`/`notifyLight`; the blinker, colours and
timeouts in `ui/notify_light`, settings `notifyLight`/`notifyTimeoutSecs`).
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
cyberspace.online's twelve, Dark … Top8, then our Bubblegum Dark and Top8 Dark, from the site's colours; picked in a
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
The scripted fake-API builds (`*_fake`, `RC_FAKE_API`) were removed on
2026-10-27 at the user's request; every firmware env talks to the real API.
The LilyGO T-LoRa Pager HAL (env `pager`) was added on 2026-10-27 (built
without flashing; `PANEL_MADCTL` and the row offset await the hardware
check; the wheel direction was flipped after a hardware check on 2026-10-28).
The LilyGO T-Deck Pro HAL (env `tdeck_pro`) was added on 2026-10-28 (built
without flashing).
The Attaky Mesh Deck HAL (env `mesh_deck`) and the notification light were
added on 2026-10-29 (flashed: it boots and finds touch, gauge and keyboard;
the panel, touch mapping, keys and LEDs await the hardware check).
**Still open:** the unticked items under "Open hardware checks" in
`docs/TESTING.md` (themes, device Settings, touch UI, Wio L2, T-Deck Plus, CrowPanels, Pager, T-Deck Pro, GPS time, API
assumptions, settings backup, OTA), then the human live checklist on the `m9` build.
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
The native suite is green: 508/508
(`pio test -e native`), and `scripts/build-upload-monitor.sh -B` builds all
eight firmware envs.

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
