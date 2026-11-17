# M5Stack Tab5 target — implementation plan

Spec: `docs/superpowers/specs/2026-11-17-cyberspace-tab5-design.md`.

**Global constraints:** AGENTS.md's hard rules. Portable logic in `lib/core`
with native tests first; hardware only in `src/hal/tab5/` behind
`src/hal/board.h`. Never call the Cyberspace API. Never flash unless the user
says so. Commit once per task (`git commit -s`, a "Tested:" line).

## Task 1: Key map (`lib/core/src/input/keymap_tab5`)

- [x] Write `test/test_tab5_input/test_main.cpp`: letters, digits, the Sym
      layer (held and tapped alone), Aa tap (one-shot), double tap (Caps Lock,
      letters only), hold; Aa as Shift on second-layer keys; Esc, Enter, Tab,
      Backspace and del with repeat, arrows with repeat; Ctrl tapped alone vs
      Ctrl+key; Alt+1…7 and Alt+0; Alt+letter types nothing; releases make
      nothing; an event byte decoded (bit 7, row, column).
- [x] Run `pio test -e native -f test_tab5_input`: fails (no header).
- [x] Implement `Tab5Keys` (`key(event, ms)`, `poll(ms)`, `capsLock()`,
      `shiftPending()`) and `kTab5FunctionKeys`.
- [x] Run it green, then the whole native suite.
- [x] Commit "Tab5: key map".

## Task 2: Battery percentage (`lib/core/src/power/battery`)

- [x] Tests in `test_battery_meter`: pack mV → percent (2S, per cell
      3300–4100 mV, clamped), below 5000 mV no reading.
- [x] Implement `rc::tab5BatteryPercent`; green; commit "Tab5: battery
      percentage".

## Task 3: Env and HAL

- [ ] `boards/tab5.json` (from `tanmatsu.json`), `[env:tab5]` in
      `platformio.ini`.
- [ ] `src/hal/tab5/`: `pins_tab5.h`; `io_tab5` (the two PI4IOE5V6408s);
      `panel_tab5` (detect, LDO, DSI, ST7123/ST7121/ILI9881C init lists,
      DPI frame, backlight PWM); `touch_tab5` (ST7123 protocol, GT911 through
      the shared driver); `keyboard_tab5` (Wire1, INT, event FIFO, LEDs);
      `audio_es8388`; `board_tab5.cpp` (rotation, 2x push, keys, wake, C6,
      restart); `peripherals_tab5.cpp` (INA226, RX8130, SD).
- [ ] `pio run -e tab5` builds, no new warnings.
- [ ] Commit "Tab5: HAL and env".

## Task 4: Scripts, CI, release, flasher

- [ ] `.github/workflows/build.yml`, `scripts/build-upload-monitor.sh`
      (`--tab5`, `-B`), `scripts/release.sh` (env list, bootloader 0x2000),
      `scripts/pages-site.sh` and `site/` board list; site tests.
- [ ] Commit "Tab5: CI, release and flasher".

## Task 5: Docs

- [ ] AGENTS.md (intro, commands, layout, progress, native count),
      README.md, docs/BUILD.md, docs/USE.md (keys), docs/TESTING.md
      (hardware check from spec §9).
- [ ] Commit "Tab5: docs".

## Task 6: Verify

- [ ] `pio test -e native`, `pio test -e native_lv`,
      `scripts/build-upload-monitor.sh -B`, `node --test site/*.test.mjs`,
      `python3 tools/pr_guidelines.py origin/main HEAD`.

## Task 7: Hardware check

- [ ] Flash when the user says to; work through the Tab5 item in
      docs/TESTING.md. Fix-ups in their own commits.
