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

- [ ] **Step 1:** The user runs spec §11's commands on the device (or allows the agent to over SSH) and pastes the output.
- [ ] **Step 2:** Record the framebuffer depth, the keyboard's evdev behaviour (raw codes vs needs xkbcommon), backlight and power-supply names, ALSA device and OS/glibc in §11; adjust §5 if anything differs.
- [ ] **Step 3: Commit** `git commit -s -m "CardputerZero spec: device probe results"`

Tasks 1, 2, 4–7 do not depend on it and may start first.

### Task 1: The Linux build skeleton and the simulator window

**Files:**
- Create: `linux/CMakeLists.txt`, `linux/CMakePresets.json`, `linux/docker/Dockerfile`, `scripts/build-cardputer-zero.sh`, `src/hal/linux_sim/board_linux_sim.cpp`, `linux/replace/main.cpp` (a stub that brings up lvui and the splash only)
- Modify: `.gitignore` (`linux/build/`)

**Interfaces:**
- Presets `sim` (host, SDL2, `BOARD_LINUX_SIM`) and `cp0` (aarch64, `BOARD_CARDPUTER_ZERO`); both add `RC_LINUX=1`, `LV_CONF_INCLUDE_SIMPLE`, `-Ilib/lvui/conf`, `-Isrc/ui/fonts`, and fetch `lvgl` v9.5.0, ArduinoJson v7.4.3, QRCode 0.0.1 by tag.
- `scripts/build-cardputer-zero.sh`: no flag builds `cp0` in the container (`docker run --platform linux/arm64`), `--sim` builds on the host, `--deb` packages; `--fake` adds `-DRC_FAKE_API=1`.

- [ ] **Step 1:** Write `CMakeLists.txt` with source lists: `lib/core/src/**`, `lib/lvui/src/**`, the cell fonts, the chosen HAL, and (later tasks) `src/**` minus the files `linux/replace/` overrides; for now only lvui, core and the HAL.
- [ ] **Step 2:** The sim HAL: SDL2 window at 3×, `pushRows` into a streaming texture, `pollKey` empty for now, every flag per spec §5.1.
- [ ] **Step 3: Run** `scripts/build-cardputer-zero.sh --sim --fake` and launch `linux/build/sim/monster-cyberspace`. Expected: the splash in a 960×510 window; closing it exits 0.
- [ ] **Step 4:** Write the Dockerfile (`debian:trixie` by digest, `build-essential cmake ninja-build libcurl4-openssl-dev libasound2-dev pkg-config git`) and run `scripts/build-cardputer-zero.sh`. Expected: an aarch64 ELF (`file` says ARM aarch64). The `cp0` HAL is a stub returning 320×170 until Task 8.
- [ ] **Step 5: Commit** `git commit -s -m "Linux build skeleton and SDL simulator window"`

### Task 2: Compatibility shims

**Files:**
- Create: `linux/compat/Arduino.h`, `linux/compat/arduino_compat.cpp`, `linux/compat/freertos/{FreeRTOS,task,semphr}.h`, `linux/compat/freertos_compat.cpp`, `linux/compat/esp_timer.h`, `linux/compat/esp_heap_caps.h`, `linux/compat/Preferences.h`, `linux/compat/preferences.cpp`, `linux/compat/FS.h`, `linux/compat/fs_compat.cpp`
- Test: `linux/tests/test_preferences.cpp`, `test_fs.cpp`, `test_freertos.cpp` (CTest)

**Interfaces:** exactly the API listed in spec §4.1, nothing more; `Preferences` root from `$XDG_CONFIG_HOME` or `~/.config`, overridable by `MONSTER_CONFIG_DIR` (tests use a temp dir).

- [ ] **Step 1: Write the failing tests:** `Preferences` put/get of every type used by `src/store/settings.cpp`, `remove`, `clear`, `isKey`, persistence across instances, file mode 0600, and a crash-safe write (a leftover `.tmp` is ignored); `FS` open/write/read/list/remove under a temp root; binary semaphore `take` with a 50 ms timeout returns false, then true after `give`; `xTaskCreatePinnedToCore` runs the function.
- [ ] **Step 2: Run** `ctest --preset sim`. Expected: build failure (missing headers).
- [ ] **Step 3: Implement** the shims.
- [ ] **Step 4: Run** `ctest --preset sim`. Expected: all pass.
- [ ] **Step 5: Commit** `git commit -s -m "Linux shims for the Arduino, FreeRTOS and ESP APIs the app uses"`

### Task 3: The CardputerZero keymap

**Files:**
- Create: `lib/core/src/input/keymap_cardputer_zero.h`, `.cpp`
- Test: `test/test_keymap_cardputer_zero/test_main.cpp`

**Interfaces:**
- `struct rc::ZeroKeyIn { uint16_t code; uint8_t value; /* 0 up 1 down 2 repeat */ uint32_t ms; };` (or keysym-based per Task 0's finding)
- `class rc::CardputerZeroKeys { bool feed(const ZeroKeyIn&, KeyEvent& out); bool quitRequested() const; };` holding shift/sym/fn state (held, latched once, locked) and the Esc hold timer (1000 ms).

- [ ] **Step 1: Write the failing tests:** every letter, digit and printed punctuation; shift; the sym layer; `fn+F/X/Z/C` arrows and plain `KEY_UP…` arrows; `fn+K/N/L/M`; `fn+1…0`/`O`/`P` to F1–F12; `fn+Backspace` Delete; Enter → Select; Tab; Backspace; short Esc → Back with no quit; Esc held 1000 ms → no Back and `quitRequested()`; latched shift applies to one key only; locked shift until tapped again; unknown codes and modifiers alone give no key.
- [ ] **Step 2: Run** `nice pio test -e native -f test_keymap_cardputer_zero`. Expected: build failure.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run** `nice pio test -e native`. Expected: all pass.
- [ ] **Step 5:** Wire the sim HAL's `pollKey`: SDL key → evdev code (Left Alt = fn, Right Alt = sym) → `CardputerZeroKeys`.
- [ ] **Step 6: Commit** `git commit -s -m "CardputerZero keymap, with the simulator's keys through it"`

### Task 4: Linux network, time and restart replacements

**Files:**
- Create: `linux/replace/net/https_client.cpp`, `sse_client.cpp`, `tls_alloc.cpp`, `wifi_mgr.cpp`, `linux/replace/app/time_service.cpp`, `restart.cpp`, `linux/replace/store/nvs_migrate.cpp`, `mascot_store.cpp`, `linux/replace/sys/mem.cpp`
- Modify: `linux/CMakeLists.txt` (swap these in for their `src/` names)
- Test: `linux/tests/test_sse_local.cpp` (SSE parse and `stop()` against a loopback server), `test_mascot_file.cpp`

**Interfaces:** the existing headers in `src/net/`, `src/app/`, `src/store/`, `src/sys/`, unchanged (spec §4.2).

- [ ] **Step 1: Write the failing tests:** SSE events from a loopback server reach `onEvent` in order, and `stop()` returns within 2 s while the server keeps the stream open; mascot save/load round trip and an empty file loads as defaults.
- [ ] **Step 2: Run** `ctest --preset sim`. Expected: failures.
- [ ] **Step 3: Implement.** libcurl: no verbose, no URL in logs (the SSE URL carries the token), system CA bundle, the same timeouts and body cap as the ESP files.
- [ ] **Step 4: Run** `ctest --preset sim`. Expected: all pass.
- [ ] **Step 5: Commit** `git commit -s -m "Linux HTTPS, SSE, Wi-Fi status, clock and mascot file"`

### Task 5: `RC_FEAT_WIFI_MGMT`

**Files:**
- Modify: `lib/core/src/feature_flags.h`, `src/screens/settings.cpp`, `src/screens/setup.cpp`, `src/screens/wifi.cpp` (whole body under the flag), `src/screens/router.cpp`, `src/app/net_worker.cpp`, `src/web/web_server.cpp`, `src/web/page.h`
- Test: extend the relevant `lib/core` test if the Settings row list lives there; otherwise a grep step.

- [ ] **Step 1:** Add `RC_FEAT_WIFI_MGMT` (default 1). Guard the setup AP, the Wi-Fi screen and its routes, network editing, the Wi-Fi LED rows, setup's Wi-Fi step and web config's Wi-Fi section.
- [ ] **Step 2: Grep check:** `grep -rn 'Id::Wifi\|setSetupAp\|scanDetailed\|setNetworks' src` shows every hit guarded.
- [ ] **Step 3: Run** `nice pio test -e native && nice pio test -e native_lv && nice pio run -e m9 && nice pio run -e cardputer_adv`. Expected: green, the same sizes as before the change.
- [ ] **Step 4: Commit** `git commit -s -m "RC_FEAT_WIFI_MGMT: compile out Wi-Fi management for boards whose OS owns it"`

### Task 6: The whole app on the simulator

**Files:**
- Modify: `linux/CMakeLists.txt` (all of `src/` minus replaced files, `src/grid_ui/`, `src/hal/<other boards>/`, `app/ota_service.cpp`, `app/power_service.cpp`), `linux/replace/main.cpp` (real `setup()` sequence), `src/app/app_state.{h,cpp}` (`requestQuit`/`quitRequested`), `src/app/ui_task.cpp` (check quit each frame), `src/hal/board.h` + `board_defaults.cpp` (`webConfigOptIn`, `backupPlaceName`), RC_LINUX guards per spec §6.

- [ ] **Step 1:** Build with `scripts/build-cardputer-zero.sh --sim --fake`; fix each compile error with a shim or a guard per spec §6 (never by editing screen logic).
- [ ] **Step 2: Run** the simulator (demo build): splash → Home → a room → type and send → C-Mail → Feed → Settings → F10 help → long Esc. Expected: each screen draws at 320×170 and the process exits 0 within 3 s of the long Esc.
- [ ] **Step 3: Run** `nice pio test -e native && nice pio test -e native_lv && scripts/build-upload-monitor.sh -B`. Expected: green (X4 only from a path without spaces).
- [ ] **Step 4: Commit** `git commit -s -m "Run the whole app on the Linux simulator"`

### Task 7: Web config opt-in and the `esp_http_server` shim

**Files:**
- Create: `linux/compat/esp_http_server.h`, `linux/compat/httpd_compat.cpp`
- Modify: `src/web/web_server.cpp` (port 8080 and the password requirement when `board::webConfigOptIn()`), `src/screens/settings.cpp` ("Web config page" item, URL and QR), `src/store/settings.cpp`, `lib/core/src/config/settings_model.h` (`webConfig`, NVS `webcfg`), `lib/core/src/config/backup_yaml.cpp` (`webConfig`)
- Test: `linux/tests/test_httpd.cpp` (routes, Basic auth 401, query parsing, chunked send, request body), `test/test_backup_yaml/` (the new key round-trips; an old file without it keeps off)

- [ ] **Step 1: Write the failing tests.**
- [ ] **Step 2: Run** `ctest --preset sim && nice pio test -e native`. Expected: failures.
- [ ] **Step 3: Implement** the shim (the `httpd_*` set listed in spec §4.1, one thread, loopback in tests) and the opt-in.
- [ ] **Step 4: Run** the tests, then the simulator: turning the item on without a password is refused; with one, `curl -u admin:<pw> http://127.0.0.1:8080/` returns the page and no password gets 401.
- [ ] **Step 5: Run** `nice pio run -e m9`. Expected: ESP web config unchanged (always on, port 80).
- [ ] **Step 6: Commit** `git commit -s -m "Web config as an opt-in on Linux, through an esp_http_server shim"`

### Task 8: The device HAL (after Task 0)

**Files:**
- Create: `src/hal/cardputer_zero/board_cardputer_zero.cpp`, `fb_cardputer_zero.cpp`, `input_cardputer_zero.cpp`, `power_cardputer_zero.cpp`, `audio_cardputer_zero.cpp`

- [ ] **Step 1:** Framebuffer: open, `FBIOGET_VSCREENINFO`, `mmap`, 16/32 bpp `pushRows`; black on exit.
- [ ] **Step 2:** Keyboard reader thread (no grab) into `CardputerZeroKeys`; device path per spec §5; xkbcommon only if Task 0 requires it.
- [ ] **Step 3:** Battery and backlight sysfs; ALSA beep thread; every flag per spec §5.
- [ ] **Step 4: Run** `scripts/build-cardputer-zero.sh`. Expected: aarch64 binary, no warnings from the new files.
- [ ] **Step 5: Commit** `git commit -s -m "CardputerZero HAL: framebuffer, keyboard, battery, backlight, beep"`

### Task 9: Packaging, release, CI and docs

**Files:**
- Create: `linux/packaging/monster-cyberspace.desktop`, `linux/packaging/icon.png` (from the existing app art), CPack settings in `linux/CMakeLists.txt`, `.github/workflows/linux.yml`
- Modify: `scripts/release.sh` (`LINUX_TARGETS`), `site/` (download note, not a flasher entry), `docs/BUILD.md`, `docs/USE.md`, `docs/TESTING.md` (open hardware check, spec §10), `AGENTS.md` (board paragraph, commands, progress), `README.md` (board list), `platformio.ini` header comment unchanged

- [ ] **Step 1: Run** `scripts/build-cardputer-zero.sh --deb`. Expected: `dist/monster-cyberspace-cardputer-zero-<version>_arm64.deb`; `dpkg-deb -c` lists `/usr/bin/monster-cyberspace`, the `.desktop` and the icon; `dpkg-deb -I` shows `Depends: libcurl4, libasound2`.
- [ ] **Step 2:** `release.sh --dist-only` builds and stages it next to the ESP images; no OTA manifest entry; `node --test site/*.test.mjs` passes.
- [ ] **Step 3:** CI workflow (arm64 device build + package artifact; host simulator build + CTest).
- [ ] **Step 4:** Docs, including the install steps (`sudo apt install ./…deb`) and the long-Esc exit.
- [ ] **Step 5: Run** `python3 tools/pr_guidelines.py origin/main HEAD`. Expected: pass.
- [ ] **Step 6: Commit** `git commit -s -m "CardputerZero: package, release, CI and docs"`

### Task 10: Hardware check (human)

- [ ] Install the `.deb` on the device when the user says to, then work through the "CardputerZero" item in `docs/TESTING.md` and the live checklist. Fix-ups go in their own commits.
