# Tanmatsu target — implementation plan

Spec: `docs/superpowers/specs/2026-11-04-cyberspace-tanmatsu-design.md`.

**Global constraints:** AGENTS.md's hard rules. Portable logic in `lib/core`
with native tests first; hardware only in `src/hal/tanmatsu/` behind
`src/hal/board.h`. Never call the Cyberspace API. Commit once per task.

## Task 1: Key map (`lib/core/src/input/keymap_tanmatsu`)

- [x] Write `test/test_tanmatsu_input/test_main.cpp`: letters, digits, Shift
      (both sides), AltGr, the three spaces, Esc/Return/Tab, arrows and
      Backspace with repeat, F1–F6, Fn+F1/F4/F5, Fn+Space, Meta tap alone vs
      Meta+key, Ctrl swallowing chars, volume up, several presses in one
      update, releases producing nothing.
- [x] Run `pio test -e native -f test_tanmatsu_input`: fails (no header).
- [x] Implement `TanmatsuKeys` (`update(state, ms)`, `next(ms)`) and
      `kTanmatsuFunctionKeys`.
- [x] Run it green, then the whole native suite.
- [x] Commit "Tanmatsu: key map".

## Task 2: Function-key table from the board

- [x] `board::functionKeys(size_t&)` in `board.h`, weak default
      `kFunctionKeys` in `board_defaults.cpp`; `renderKeysHelp` uses it, key
      column 5 cells.
- [x] Build `m9` and `tdisplay_p4`.
- [x] Commit "Function-key help table from the board".

## Task 3: Env and board file

- [ ] `boards/tanmatsu.json`, `[env:tanmatsu]` in `platformio.ini`.
- [ ] `src/hal/tanmatsu/pins_tanmatsu.h`, `coproc_tanmatsu.{h,cpp}`
      (register reads/writes over Wire with a mutex), `panel_st7701.{h,cpp}`,
      `audio_es8156.{h,cpp}`, `board_tanmatsu.cpp`,
      `peripherals_tanmatsu.cpp` (battery, RTC, SD).
- [ ] `pio run -e tanmatsu` builds.
- [ ] Commit "Tanmatsu: HAL and env".

## Task 4: Scripts and CI

- [ ] `.github/workflows/build.yml` env list, `scripts/build-upload-monitor.sh`
      `--tanmatsu`/`-T` and its `-B` list.
- [ ] Commit "Tanmatsu: CI and build script".

## Task 5: Docs

- [ ] AGENTS.md (intro, commands, layout, progress, native count),
      docs/BUILD.md, docs/USE.md (keys), docs/TESTING.md (hardware checks).
- [ ] Commit "Tanmatsu: docs".

## Task 6: Verify

- [ ] `pio test -e native`, `scripts/build-upload-monitor.sh -B`.
