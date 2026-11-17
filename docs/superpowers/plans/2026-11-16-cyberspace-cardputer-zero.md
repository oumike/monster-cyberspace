# CardputerZero Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Monster for Cyberspace as an aarch64 Linux app (`.deb`) for the M5Stack CardputerZero, plus a desktop simulator, reusing `lib/core`, `lib/lvui` and `src/` unchanged except a few flagged spots.

**Architecture:** A CMake build in `linux/` compiles the existing sources against small shims for the Arduino/ESP-IDF/FreeRTOS APIs `src/` uses (`linux/compat/`), Linux versions of the ESP-only files (`linux/replace/`), and one of two HALs: `src/hal/cardputer_zero/` (fbdev, evdev, sysfs, ALSA) or `src/hal/linux_sim/` (SDL2). Portable logic (the keymap) lives in `lib/core` with native tests.

**Tech Stack:** CMake ≥ 3.25, C++17, LVGL 9.5.0, libcurl, ALSA, SDL2 (simulator), Debian trixie arm64 container, PlatformIO native tests (Unity), CTest.

**Spec:** `docs/superpowers/specs/2026-11-16-cyberspace-cardputer-zero-design.md`

## Global Constraints

- Read `AGENTS.md` first. Its hard rules bind every task: never call the live Cyberspace API (agents run the simulator only with `-DRC_FAKE_API=1`); every write from a key press; no captive portal; never log passwords or tokens.
- Follow `CONTRIBUTING.md`: `git commit -s`, an SPDX header on new source files, exact dependency versions (CMake `FetchContent` by tag, container image by digest), one topic per commit; run `python3 tools/pr_guidelines.py origin/main HEAD` before asking to push.
- Every PlatformIO env must still build, with today's behaviour: new flags default to today's value; `src/grid_ui/` is never touched.
- Hardware access outside `src/hal/` only through `src/hal/board.h`.
- Board macros `BOARD_CARDPUTER_ZERO`, `BOARD_LINUX_SIM`; both define `RC_LINUX=1`. Board name "CardputerZero", slug `cardputer-zero`, package `monster-cyberspace`.
- Build with `nice`. Never install on or touch a device unless the user says so in that request. Commit on the current branch with the trailer `Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>`. Do not push.
- Commands: `nice pio test -e native`, `nice pio test -e native_lv`, `nice pio run -e <env>`, `scripts/build-cardputer-zero.sh [--sim] [--deb]`, `ctest --preset sim` (in `linux/`).

## Review Focus

- A long Esc must quit cleanly: workers stopped, SSE thread joined within 2 s, settings flushed, framebuffer left black. A short Esc must be Back, never quit. (Task 3 tests, Task 9 walk-through.)
- `Preferences` writes must be atomic: kill the process mid-save and the old file must still load. (Task 2 test.)
- The `esp_http_server` shim must reject a request without the page password, and on Linux the server must refuse to start without one. (Task 7.)
- Turning `RC_FEAT_WIFI_MGMT` off must leave no path to a Wi-Fi screen or setup AP: grep every `Id::Wifi`/`setSetupAp` caller. (Task 5.)
- No new log line may print the token, the refresh token or a password; the libcurl verbose flag stays off. (Tasks 4, 6.)

---

### Task 0: Device probe (human, before Tasks 3 and 8)

**Files:**
- Modify: `docs/superpowers/specs/2026-11-16-cyberspace-cardputer-zero-design.md` §11 (write the answers in)

M5Stack's docs and sources already settled the input form (raw evdev; the kernel applies the modifiers), the framebuffer's name and the launcher's `SIGINT` (spec §2, §2.1), so Tasks 3 and 8 may start without it; this probe confirms them.

- [ ] **Step 1:** The user runs spec §11's commands on the device (or allows the agent to over SSH) and pastes the output.
- [ ] **Step 2:** Record the framebuffer depth, any key codes that differ from spec §2 (one-shot and locked shift, `fn+U/I`, `fn+H`), backlight and power-supply names, ALSA device, OS/glibc and the app's user in §11; adjust §5 and the keymap if anything differs.
- [ ] **Step 3: Commit** `git commit -s -m "CardputerZero spec: device probe results"`

Tasks 1, 2, 4–7 do not depend on it and may start first.

### Task 1: The Linux build skeleton and the simulator window

**Files:**
- Create: `linux/CMakeLists.txt`, `linux/CMakePresets.json`, `linux/docker/Dockerfile`, `scripts/build-cardputer-zero.sh`, `linux/replace/linux_hal.h` (`linuxhal::runMainLoop`, `end`), `src/hal/cardputer_zero/flags_cardputer_zero.cpp` (the board's flags, built by both HALs), `src/hal/cardputer_zero/board_cardputer_zero.cpp` (stub), `src/hal/linux_sim/board_linux_sim.cpp`, `linux/replace/main.cpp` (a stub that brings up lvui and a name label only)
- Modify: `.gitignore` (`linux/build/`), `lib/lvui/conf/lv_conf.h` (libc allocator for `RC_LINUX`)

**Interfaces:**
- Presets `sim` (host, SDL2, `BOARD_LINUX_SIM`) and `cp0` (aarch64, `BOARD_CARDPUTER_ZERO`); both add `RC_LINUX=1`, `BOARD_HAS_PSRAM=1` (whole frame, full LVGL), `LV_CONF_INCLUDE_SIMPLE`, `-Ilib/lvui/conf`, `-Isrc/ui/fonts`, and fetch `lvgl` v9.5.0, ArduinoJson v7.4.3 and QRCode 0.0.1 (its one release, commit eafbde4) as tarballs pinned by SHA-256.
- `scripts/build-cardputer-zero.sh`: no flag builds `cp0` in the container (`docker run --platform linux/arm64`), `--sim` builds on the host, `--fake` adds `-DRC_FAKE_API=1` (into `build/<target>-fake`), `--clean`; Task 9 adds `--deb`.

- [x] **Step 1:** Write `CMakeLists.txt` with source lists: `lib/core/src/**`, `lib/lvui/src/**`, the cell fonts, the chosen HAL, and (later tasks) `src/**` minus the files `linux/replace/` overrides; for now only lvui, core and the HAL.
- [x] **Step 2:** The sim HAL: SDL2 window at 3×, `pushRows` into a shared frame the main thread presents (macOS lets only it touch SDL), `pollKey` empty for now, every flag per spec §5.1. `MONSTER_SIM_SHOT=<file.bmp>` writes the last frame on exit, so agents can check the UI without seeing the window.
- [x] **Step 3: Run** `scripts/build-cardputer-zero.sh --sim --fake` and launch `linux/build/sim-fake/monster-cyberspace`. Expected: the name label in a 960×510 window; closing it (or SIGTERM) exits 0.
- [x] **Step 4:** Write the Dockerfile (`debian:trixie` by digest, `build-essential ca-certificates cmake ninja-build pkg-config file libcurl4-openssl-dev libasound2-dev`) and run `scripts/build-cardputer-zero.sh`. Expected: an aarch64 ELF (`file` says ARM aarch64). The `cp0` HAL is a stub returning 320×170 until Task 8.
- [x] **Step 5: Commit** `git commit -s -m "Linux build skeleton and SDL simulator window"`

### Task 2: Compatibility shims

**Files:**
- Create: `linux/compat/Arduino.h`, `linux/compat/arduino_compat.cpp`, `linux/compat/freertos/{FreeRTOS,task,semphr}.h`, `linux/compat/freertos_compat.cpp`, `linux/compat/esp_err.h`, `linux/compat/esp_timer.h`, `linux/compat/esp_heap_caps.h`, `linux/compat/Preferences.h`, `linux/compat/preferences.cpp`, `linux/compat/FS.h`, `linux/compat/fs_compat.cpp`
- Test: `linux/tests/check.h`, `test_preferences.cpp`, `test_fs.cpp`, `test_rtos.cpp` (CTest, wired in `linux/CMakeLists.txt` as `mc_compat` plus one executable per test)

**Interfaces:** exactly the API listed in spec §4.1, nothing more; `Preferences` root from `$XDG_CONFIG_HOME` or `~/.config`, overridable by `MONSTER_CONFIG_DIR` (tests use a temp dir).

- [x] **Step 1: Write the failing tests:** `Preferences` put/get of every type used by `src/store/settings.cpp`, `remove`, `clear`, `isKey`, persistence across instances, file mode 0600, and a crash-safe write (a leftover `.tmp` is ignored); `FS` open/write/read/list/remove under a temp root; mutex `take` with a 50 ms timeout returns false while held, then true after `give` (only mutexes are used); `xTaskCreatePinnedToCore` runs the function and `xTaskGetHandle` finds it until it returns; `millis`/`delay`/`vTaskDelay`; `random` ranges; a periodic `esp_timer` ticks and stops; the heap queries report plenty.
- [x] **Step 2: Run** `ctest --preset sim`. Expected: build failure (missing headers).
- [x] **Step 3: Implement** the shims.
- [x] **Step 4: Run** `ctest --preset sim`. Expected: all pass.
- [x] **Step 5: Commit** `git commit -s -m "Linux shims for the Arduino, FreeRTOS and ESP APIs the app uses"`

### Task 3: The CardputerZero keymap

**Files:**
- Create: `lib/core/src/input/keymap_cardputer_zero.h`, `.cpp` (with the `sym` keymap-file parser and the built-in table)
- Test: `test/test_keymap_cardputer_zero/test_main.cpp`

**Interfaces:**
- `struct rc::ZeroKeyIn { uint16_t code; uint8_t value; /* 0 up 1 down 2 repeat */ uint32_t ms; uint8_t mods; /* kZeroModShift/kZeroModFn from modifier_state, 0 if unknown */ };`; evdev codes as `rc::zerokey::k*`.
- `class rc::CardputerZeroKeys { bool feed(const ZeroKeyIn&, KeyEvent& out); bool poll(uint32_t ms, KeyEvent& out); bool quitRequested() const; ZeroSystemKey takeSystemKey(); bool loadSymMap(const char* text); };` tracking held `KEY_LEFTSHIFT`/`KEY_FN`, repeats and the Esc hold timer (1000 ms); no latching (the kernel does it, spec §2).
- `rc::kCardputerZeroFunctionKeys`: F10's table, returned by both HALs' `board::functionKeys` (`flags_cardputer_zero.cpp`).

- [x] **Step 1: Write the failing tests:** every letter, digit and printed punctuation; held `KEY_LEFTSHIFT` and the `mods` shift bit both give capitals; the built-in `sym` table and a parsed keymap file (comments, bad lines, unknown keysyms skipped); `KEY_UP…` arrows and the `fn+F/X/Z/C` fallback; `KEY_HOME/END/PAGEUP/PAGEDOWN`; `KEY_F1…F12`; `KEY_DELETE`; Enter → Select; Tab; Backspace; short Esc → Back with no quit; Esc held 1000 ms → no Back and `quitRequested()`; repeats of arrows and Backspace; unknown codes and modifiers alone give no key.
- [x] **Step 2: Run** `nice pio test -e native -f test_keymap_cardputer_zero`. Expected: build failure.
- [x] **Step 3: Implement.**
- [x] **Step 4: Run** `nice pio test -e native`. Expected: all pass.
- [x] **Step 5:** Wire the sim HAL's `pollKey` (and, until Task 6, show the last key on Task 1's screen): SDL key → the device's evdev code (spec §5.1: Shift = `KEY_LEFTSHIFT`, Left Alt = `KEY_FN`, typed symbols as `sym` codes) → `CardputerZeroKeys`.
- [x] **Step 6: Commit** `git commit -s -m "CardputerZero keymap, with the simulator's keys through it"`

### Task 4: Linux network, time and restart replacements

**Files:**
- Create: `linux/replace/net/https_client.cpp`, `sse_client.cpp`, `tls_alloc.cpp`, `wifi_mgr.cpp`, `linux/replace/app/time_service.cpp`, `restart.cpp`, `linux/replace/store/nvs_migrate.cpp`, `mascot_store.cpp`, `linux/replace/sys/mem.cpp`
- Modify: `linux/CMakeLists.txt` (swap these in for their `src/` names)
- Test: `linux/tests/test_sse_local.cpp` (SSE parse and `stop()` against a loopback server), `test_mascot_file.cpp`

**Interfaces:** the existing headers in `src/net/`, `src/app/`, `src/store/`, `src/sys/`, unchanged (spec §4.2).

- [x] **Step 1: Write the failing tests:** SSE events from a loopback server reach `onEvent` in order, and `stop()` returns within 2 s while the server keeps the stream open; mascot save/load round trip and an empty file loads as defaults.
- [x] **Step 2: Run** `ctest --preset sim`. Expected: failures.
- [x] **Step 3: Implement.** libcurl: no verbose, no URL in logs (the SSE URL carries the token), system CA bundle, the same timeouts and body cap as the ESP files.
- [x] **Step 4: Run** `ctest --preset sim`. Expected: all pass.
- [x] **Step 5: Commit** `git commit -s -m "Linux HTTPS, SSE, Wi-Fi status, clock and mascot file"`

### Task 5: `RC_FEAT_WIFI_MGMT`

**Files:**
- Modify: `lib/core/src/feature_flags.h`, `src/screens/settings.cpp`, `src/screens/setup.cpp`, `src/screens/wifi.cpp` (whole body under the flag), `src/screens/router.cpp`, `src/app/net_worker.cpp`, `src/web/web_server.cpp`, `src/web/page.h`
- Test: extend the relevant `lib/core` test if the Settings row list lives there; otherwise a grep step.

- [x] **Step 1:** Add `RC_FEAT_WIFI_MGMT` (default 1). Guard the setup AP, the Wi-Fi screen and its routes, network editing, the Wi-Fi LED rows, setup's Wi-Fi step and web config's Wi-Fi section.
- [x] **Step 2: Grep check:** `grep -rn 'Id::Wifi\|setSetupAp\|scanDetailed\|setNetworks' src` shows every hit guarded.
- [x] **Step 3: Run** `nice pio test -e native && nice pio test -e native_lv && nice pio run -e m9 && nice pio run -e cardputer_adv`. Expected: green, the same sizes as before the change. (Done: RAM unchanged; flash 128 B smaller on the m9 and 124 B on the ADV, from `setup.cpp` building its lists with `push_back`. The config page bytes are identical. With the flag at 0 the m9 builds too.)
- [x] **Step 4: Commit** `git commit -s -m "RC_FEAT_WIFI_MGMT: compile out Wi-Fi management for boards whose OS owns it"`

### Task 6: The whole app on the simulator

**Files:**
- Modify: `linux/CMakeLists.txt` (all of `src/` minus replaced files, `src/grid_ui/`, `src/hal/<other boards>/`, `app/ota_service.cpp`, `app/power_service.cpp`), `linux/replace/main.cpp` (real `setup()` sequence), `src/app/app_state.{h,cpp}` (`requestQuit`/`quitRequested`), `src/app/ui_task.cpp` (check quit each frame), `src/hal/board.h` + `board_defaults.cpp` (`webConfigOptIn`, `backupPlaceName`), RC_LINUX guards per spec §6.

- [x] **Step 1:** Build with `scripts/build-cardputer-zero.sh --sim --fake`; fix each compile error with a shim or a guard per spec §6 (never by editing screen logic). (Done: web config is a stand-in that serves nothing until Task 7, `linux/replace/web/web_server.cpp`; `power_service.cpp` builds as is. `webConfigOptIn` and `backupPlaceName` move to Tasks 7 and 8, where they are used.)
- [x] **Step 2: Run** the simulator (demo build): splash → Home → a room → type and send → C-Mail → Feed → Settings → F10 help → long Esc. Expected: each screen draws at 320×170 and the process exits 0 within 3 s of the long Esc. (Done through the demo console, which gained `key settings` and `key help`: every screen drew at 320×170, and SIGTERM exits 0 in 0.5 s. Holding Esc in the window is still to be tried by hand.)
- [x] **Step 3: Run** `nice pio test -e native && nice pio test -e native_lv && scripts/build-upload-monitor.sh -B`. Expected: green (X4 only from a path without spaces). (Done: native and native_lv green; m9, wio_l2, tdeck and crowpanel35 built, the other envs skipped at the user's call; the CardputerZero container build and its ctest pass.)
- [x] **Step 4: Commit** `git commit -s -m "Run the whole app on the Linux simulator"`

### Task 7: Web config opt-in and the `esp_http_server` shim

**Files:**
- Create: `linux/compat/esp_http_server.h`, `linux/compat/httpd_compat.cpp`
- Modify: `src/web/web_server.cpp` (port 8080 and the password requirement when `board::webConfigOptIn()`), `src/screens/settings.cpp` ("Web config page" item, URL and QR), `src/store/settings.cpp`, `lib/core/src/config/settings_model.h` (`webConfig`, NVS `webcfg`), `lib/core/src/config/backup_yaml.cpp` (`webConfig`)
- Test: `linux/tests/test_httpd.cpp` (routes, Basic auth 401, query parsing, chunked send, request body), `test/test_backup_yaml/` (the new key round-trips; an old file without it keeps off)

- [x] **Step 1: Write the failing tests.** (Done: `linux/tests/test_httpd.cpp`. The backup half already exists: `test_missing_keys_keep_current` keeps `webConfig` from an old file, and the round trip covers the key.)
- [x] **Step 2: Run** `ctest --preset sim && nice pio test -e native`. Expected: failures. (Done: a link failure.)
- [x] **Step 3: Implement** the shim (the `httpd_*` set listed in spec §4.1, one thread, loopback in tests) and the opt-in. (Done: a compile-time `RC_WEB_OPT_IN` instead of `board::webConfigOptIn()`, so ESP images don't change (spec §6), and a `WebConfigScreen` for the password and the switch.)
- [x] **Step 4: Run** the tests, then the simulator: turning the item on without a password is refused; with one, `curl -u admin:<pw> http://127.0.0.1:8080/` returns the page and no password gets 401. (Done in the demo sim: refused without a password, 401 with none or a wrong one, 200 with it, nothing once off. The QR code needs 15 rows; the 320x170 panel has 14, so it shows only on bigger windows.)
- [x] **Step 5: Run** `nice pio run -e m9`. Expected: ESP web config unchanged (always on, port 80). (Done: RAM 72,116 B and flash 3,170,454 B, the same as before.)
- [x] **Step 6: Commit** `git commit -s -m "Web config as an opt-in on Linux, through an esp_http_server shim"`

### Task 8: The device HAL (after Task 0)

**Files:**
- Create: `src/hal/cardputer_zero/board_cardputer_zero.cpp`, `fb_cardputer_zero.cpp`, `input_cardputer_zero.cpp`, `power_cardputer_zero.cpp`, `audio_cardputer_zero.cpp`

- [x] **Step 1:** Framebuffer: pick the node (spec §5: `LV_LINUX_FBDEV_DEVICE`, `/proc/fb`'s `fb_st7789v`, `/dev/fb0`), `FBIOGET_VSCREENINFO`, `mmap`, 16/32 bpp `pushRows`; black on exit.
- [x] **Step 2:** Keyboard reader thread (no grab, raw evdev) into `CardputerZeroKeys`; device path per spec §5; `modifier_state` read on each press; the `sym` table from `/usr/share/keymaps/tca8418_keypad_m5stack_keymap.map`; `SIGINT`/`SIGTERM` set the quit flag.
- [x] **Step 3:** Battery and backlight sysfs; ALSA beep thread; every flag per spec §5.
- [x] **Step 4: Run** `scripts/build-cardputer-zero.sh`. Expected: aarch64 binary, no warnings from the new files. *Done: no warnings with `-Wall -Wextra` on the HAL files; links `libasound.so.2` and `libcurl.so.4`; container ctest passes. Backups go to `~/monster-cyberspace` through `sdMount` (spec §8). Not run on hardware yet (Task 10).*
- [x] **Step 5: Commit** `git commit -s -m "CardputerZero HAL: framebuffer, keyboard, battery, backlight, beep"`

### Task 9: Packaging, release, CI and docs

**Files:**
- Create: `linux/packaging/monster-cyberspace.desktop`, `linux/packaging/icon.png` (from the existing app art), CPack settings (no `postinst`/`postrm`, see Step 1) in `linux/CMakeLists.txt`, `.github/workflows/linux.yml`
- Modify: `scripts/release.sh` (`LINUX_TARGETS`), `site/` (download note, not a flasher entry), `docs/BUILD.md`, `docs/USE.md`, `docs/TESTING.md` (open hardware check, spec §10), `AGENTS.md` (board paragraph, commands, progress), `README.md` (board list), `platformio.ini` header comment unchanged

- [x] **Step 1: Run** `scripts/build-cardputer-zero.sh --deb`. Expected: `dist/monster-cyberspace-cardputer-zero-<version>_arm64.deb`; `dpkg-deb -c` lists `/usr/share/APPLaunch/bin/monster-cyberspace`, the `.desktop` and `share/images/monster-cyberspace.png`; `dpkg-deb -I` shows `Section: APPLaunch`, `Architecture: arm64`, `Depends: libcurl4, libasound2`, and the `postinst`/`postrm` that restart `APPLaunch.service`. *Done, with three changes (spec §9): no `postinst`/`postrm` (M5's packaging guide: APPLaunch is a user service that rescans `applications/` every few seconds); `Depends: libcurl4t64 | libcurl4, libasound2t64 | libasound2` (trixie's t64 names); the file is `…-v2.3.2_arm64.deb`, the tag form of the ESP assets. The icon is drawn by `linux/packaging/make_icon.py` (100×100, the stock icons' size), and `LICENSE` ships as the copyright file. `apt install` in a clean trixie arm64 container resolved the dependencies, `ldd` found everything, and removal left nothing.*
- [x] **Step 2:** `release.sh --dist-only` builds and stages it next to the ESP images; no OTA manifest entry; `node --test site/*.test.mjs` passes. *Done: `--dist-only --only cardputer_zero --no-clean` staged the deb with `SHA256SUMS` and `BUILD-INFO.txt`; `--check-targets` lists it; a local release attaches it, and in `release.yml` a `linux` job builds it from the tag on `ubuntu-24.04-arm` and uploads it and its `SHA256SUMS` line. The site has a download note; site tests 7/7.*
- [x] **Step 3:** CI workflow (arm64 device build + package artifact; host simulator build + CTest). *Done: `.github/workflows/linux.yml`; the simulator job's steps built and passed ctest 6/6 in an `ubuntu:24.04` container locally.*
- [x] **Step 4:** Docs, including the install steps (`sudo apt install ./…deb`) and the long-Esc exit. *Done: BUILD.md, USE.md, TESTING.md (the CardputerZero hardware check and the CTest suite), AGENTS.md, README.md, spec §9.*
- [x] **Step 5: Run** `python3 tools/pr_guidelines.py origin/main HEAD`. Expected: pass.
- [x] **Step 6: Commit** `git commit -s -m "CardputerZero: package, release, CI and docs"`

### Task 10: Hardware check (human)

- [x] Read-only probes over SSH (2026-11-17), answers in spec §11. *Fix-up: the beep goes through PulseAudio (ALSA's default is HDMI), and the panel is found as `/dev/fb_lcd` or `panel-mipi-dbid`.*
- [x] Install the `.deb` on the device when the user says to, then work through the "CardputerZero" item in `docs/TESTING.md` and the live checklist. Fix-ups go in their own commits. *Done 2026-11-17: every app item passed on the device.*
