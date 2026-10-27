# Rhino Cyberspace — agent instructions

Standalone ESP32 firmware client for [cyberspace.online](https://cyberspace.online),
a retro text-only social network. Milestone 1 is cIRC chat plus a device-served
web config page, on the Elecrow ThinkNode M9. The Seeed Wio Tracker L2 and
the LilyGO T-Deck Plus followed (built, not yet tried on hardware). More
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
- **No captive portal.** The setup AP (`Rhino-Cyberspace-<BOARD>-Setup`) is
  open unless a password is configured in web config. The page is served at
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
scripts/build-upload-monitor.sh m9   # flash + serial monitor (115200); -w for the Wio L2, -t for the T-Deck Plus; --help for options
scripts/build-upload-monitor.sh -B   # build every firmware env, no device needed
```

## Layout

`lib/core/` holds portable, Arduino-free logic with native tests in `test/`.
`src/` holds networking, tasks, screens, and the web server; each board's HAL
sits behind `src/hal/board.h` in `src/hal/<board>/` (boards return portable
keys from `board::pollKey`). M9 hardware references live in the sibling
projects `../../camillia/camillia-mt/src/hal/hw_m9.h` and `../../rhino-moshi/firmware/`.
`src/hal/wio_l2/` is the Seeed Wio Tracker L2, ported from
`../../camillia/camillia-mt` (`src/hal/hw_wio_tracker_l2.h`, `wio_tracker_l2_io.cpp`)
and `../platy-aprs/src/hal/` (`pins_wio_l2.h`, `wio_l2_io.cpp`, `audio.cpp`).
`src/hal/tdeck/` is the LilyGO T-Deck Plus (from camillia-mt's `hw_tdeck.h`). It
reuses the M9's `st7789` and the Wio's `gt911` drivers through config structs,
and sets the `hasKeyboard`, `backspaceIsBack` and `anyInputWakes` flags; its
input rules (`keymap_tdeck`, `backspace`, `click_timer`) live in `lib/core`.
Touch: `lib/core` turns samples into gestures and resolves them against the
per-frame `TapMap` widgets fill as they draw (`input/gestures`, `ui/tap_map`,
`input/touch_resolve`, the `Osk` keyboard); `ui_task` feeds `handleKey` and
docks the keyboard. Boards without touch register nothing.

## Progress (hand-off from Claude Code, 2026-10-25)

Work is on **`main`** at https://github.com/oumike/platy-cyberspace. All 22 tasks of the M1 plan, all 6 of the
M2 (C-Mail) plan and all 12 of the M3 (Feed) plan are done and committed, with
their boxes ticked. The room-screen, C-Mail, web config and Feed (`m9_fake`)
hardware checks passed on the device (2026-10-26).
Also done since: themes (Default / Black & white / White & black, picked in
Settings and web config), and the Wio Tracker L2 HAL with env `wio_l2`
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
The scripted fake-API builds (`*_fake`, `RC_FAKE_API`) were removed on
2026-10-27 at the user's request; every firmware env talks to the real API.
**Still open:** the unticked items under "Open hardware checks" in
`docs/TESTING.md` (themes, device Settings, touch UI, Wio L2, T-Deck Plus, GPS time, API
assumptions, settings backup), then the human live checklist on the `m9` build.
CI (`.github/workflows/build.yml`) runs on GitHub.
The native suite is green: 358/358
(`pio test -e native`), and `scripts/build-upload-monitor.sh -B` builds all
three firmware envs.

Notes for whoever continues:
- Each task follows test-driven development: write the plan's tests first, run
  them and watch them fail, implement, run them green, then commit with the
  plan's commit message.
- Cell fonts: `src/ui/fonts/cell*.h` are generated by `scripts/bdf2cells.py`
  from Terminus 4.49.1 (download outside the repo, run with `python3 -I`).
  Glyphs Terminus lacks are aliased in `lib/core/src/ui/glyph_set.h`.
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
