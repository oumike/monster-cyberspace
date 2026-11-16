# CardputerZero — design

Date: 2026-11-16. Status: approved in chat, awaiting spec review.

## 1. Goal

Monster for Cyberspace on the M5Stack CardputerZero. The CardputerZero is
not an ESP32: it is a pocket Linux computer on a Raspberry Pi CM0, so this is
the project's first **Linux port**. The result is a program, not a firmware
image. It is an aarch64 Linux executable packaged as a `.deb`, which the
CardputerZero launcher (APPLaunch) lists and starts like its other apps.

Success: the `.deb` installs on the device, the launcher shows Monster, and
it starts full screen. Sign-in, settings and every feature but OTA work from
the keyboard. A long Esc returns to the launcher. Before any device is used, a
desktop simulator build runs the same code in a 320×170 window against the
demo source (`RC_FAKE_API`).

User decisions (2026-11-16):

- **Write a spec and plan first**, before any code.
- **Features: everything except OTA.** Updates come as a new `.deb`. The
  Fluffy mascot is stored in a file instead of a flash partition.
- **Web config: included, off by default.** It is turned on in device
  Settings and needs a page password before it starts. It listens on the
  device's LAN address (§7).
- **Desktop simulator: yes** (SDL2, macOS and Linux hosts).
- **Distribution: attach the `.deb` to GitHub releases.** Submitting to the
  CardputerZero Store comes later (§12).

## 2. Hardware and platform

From M5Stack's docs (`docs.m5stack.com/en/CardputerZero`, the usage guide)
and the `CardputerZero/Template` and `CardputerZero/launcher` repositories:

| Part | Details | How the port reaches it |
| --- | --- | --- |
| SoC | RP3A0 (BCM2837), 4× Cortex-A53 @ 1 GHz, 512 MB LPDDR2 | aarch64 Linux (Raspberry Pi OS / Debian 13 "trixie" based) |
| Display | 1.9" ST7789v3, 320×170, SPI (CM0 GPIO 10/11/25/8, TE 5); reset and backlight PWM on the M5IOE1 expander | the kernel's framebuffer, `/dev/fb0` (launcher and Template use LVGL's fbdev path) |
| Keyboard | 46 keys, TCA8418 at 0x34 on I²C1 (GPIO 2/3), INT GPIO 27; `shift`, `sym`, `fn` modifiers (hold, tap once to latch, tap twice to lock) | evdev, `/dev/input/by-path/platform-3f804000.i2c-event` (the launcher's default) |
| Wi-Fi / Ethernet | 2.4 GHz b/g/n; USB Ethernet in Host mode | managed by Linux (NetworkManager); the app only reads the state |
| Battery | 1750 mAh, BQ27220 gauge | power-supply sysfs (`/sys/class/power_supply/*/capacity`, `status`) |
| RTC | RX8130CE | the system clock; Linux keeps NTP and the RTC |
| Audio | ES8389 codec, AW8737A amp (EN on GPIO 24), headphone detect GPIO 17 | ALSA (default PCM) |
| Backlight | PWM on M5IOE1 | backlight sysfs (`/sys/class/backlight/*`) if exposed; otherwise unsupported (probe, §11) |
| Storage | microSD is the root filesystem | ordinary files under the user's home |
| Not used | camera, IMU, magnetometer, IR, HDMI, Cap bus, Grove | — |

Fn layer (from the usage guide): `fn+F/X/Z/C` are Up/Down/Left/Right,
`fn+K/N` Home/End, `fn+L/M` PgUp/PgDn, `fn+1…0` F1…F10, `fn+O/P` F11/F12,
`fn+Backspace` Delete, `fn+U/I` brightness, `fn+S/D` volume, `fn+H` app
help. **Unknown until a device is probed:** whether the kernel keymap or a
userspace layer (the launcher's libinput + xkbcommon reader) applies `sym`
and `fn`. That decides whether we see `KEY_UP` or `KEY_F` + an `fn` code. §5
handles both, and §11 lists the probe.

### 2.1 The launcher contract

- Apps are `.desktop` files in `/usr/share/APPLaunch/applications/`
  (`Name`, `Exec`, `Icon`, `Terminal=false`). The launcher polls that
  directory every 3 s.
- Starting an app runs `ExecBlocking`: the launcher clears its input group,
  stops its LVGL timers and waits for the process to exit, then redraws
  home. The app owns `/dev/fb0` and the keyboard while it runs.
- The launcher keeps reading the keyboard and records Esc state in a shared
  atomic. A **long Esc** is the system-wide "back to home" gesture and the
  launcher may kill an app that ignores it. Our app exits by itself on a long
  Esc (§5) so it never has to be killed.
- Apps run as the logged-in user (`run_as_user`), not root. That user must
  be able to open `/dev/fb0` and the input device; stock apps do, and §11 checks it.

## 3. Architecture

The ESP builds stay exactly as they are. The Linux port adds a second build
system, a set of **compatibility shims**, and a few **Linux replacements**
for the ESP-only files. The rest of `src/`, all of `lib/core` and all of
`lib/lvui` compile unchanged.

```
lib/core/            unchanged (already builds on the host for tests)
lib/lvui/            unchanged (already builds on the host for native_lv)
src/                 unchanged except the guarded spots in §6
src/hal/cardputer_zero/   board.h for the device (fbdev, evdev, sysfs, ALSA)
src/hal/linux_sim/        board.h for the desktop simulator (SDL2)
linux/
  CMakeLists.txt     the Linux build (device and simulator)
  CMakePresets.json  cp0 (device, aarch64) and sim (host) presets
  compat/            header-only and small .cpp shims for ESP/Arduino APIs
  replace/           Linux versions of ESP-only src/ files (§4.2)
  packaging/         .desktop, icon, cpack settings
```

`BOARD_CARDPUTER_ZERO` selects the device HAL and `BOARD_LINUX_SIM` the
simulator HAL. Both define `RC_LINUX=1`, which only the shims, the
replacements and the §6 guards test. Hardware access still goes through
`src/hal/board.h` only; the hard rule about `src/hal/<board>/` headers holds.

### 3.1 Why shims rather than a refactor

About 24 files outside `src/hal/` include Arduino, ESP-IDF or FreeRTOS
headers, but they use a small surface: `millis`, `delay`, `Serial.printf`,
`Preferences`, `xTaskCreatePinnedToCore`, three semaphore calls, `vTaskDelay`,
`esp_timer_get_time`, `heap_caps_*`, and a few `esp_http_server` calls. A
shim for each keeps one copy of the app logic for every board. Refactoring
`src/` behind new interfaces would touch every ESP board for no gain to
them. Files whose whole job is ESP-specific (HTTP client, SSE client, Wi-Fi,
SNTP, OTA, NVS migration, flash partitions) get a Linux replacement instead
of a shim.

## 4. Linux layer

### 4.1 Shims — `linux/compat/`

Each shim implements only what `src/` uses (counted 2026-11-16, Task 2) and
fails to compile on anything else, so a new use is noticed. CTest covers
them (`linux/tests/`, `ctest --preset sim`, and in the container):

| Header | Provides | Backed by |
| --- | --- | --- |
| `Arduino.h` | `millis`, `micros`, `delay`, `yield`, `random(max)`, `random(min, max)`, `esp_random`, `Serial` (`begin`, `printf`, `print`, `println`, `write`, `read`, `available`, `flush`), `ESP.restart`, a minimal `String` (what `Preferences::getString` returns); includes the FreeRTOS headers, as the ESP core's does | `std::chrono`, `<random>` and `std::random_device`, stdout and non-blocking stdin (end of input stops polling), `execv` of itself with the arguments `main()` passes to `compat::setArgs` |
| `freertos/FreeRTOS.h`, `task.h`, `semphr.h` | `xTaskCreate(PinnedToCore)`, `vTaskDelay`, `xTaskGetTickCount`, `xTaskGetHandle`, `uxTaskGetStackHighWaterMark` (a constant; logs only), `pdMS_TO_TICKS`, `portMAX_DELAY`, `portNUM_PROCESSORS` (2), mutexes (`xSemaphoreCreateMutex`, `Take` with a timeout, `Give`, `vSemaphoreDelete`); no binary semaphores or queues, which nothing uses | detached `std::thread`s named after the task (a task ends when its function returns), `std::timed_mutex` |
| `esp_timer.h` (+ `esp_err.h`) | `esp_timer_get_time`, `esp_timer_create`, `start_once`, `start_periodic`, `stop`, `delete` | `steady_clock`, one thread per started timer |
| `esp_heap_caps.h` | `heap_caps_malloc/calloc/realloc/free`, the size queries (the system's free memory, at most `UINT_MAX`, so the low-memory paths never trigger), the `MALLOC_CAP_*` flags | libc, `sysinfo` |
| `Preferences.h` | `begin` (read-only fails on a namespace never created), `end`, `clear`, `remove`, `isKey`, `put`/`get` `Bool`, `UChar`, `UShort`, `UInt`, `String`; keys over 15 characters fail as in NVS | one text file per namespace in `$MONSTER_CONFIG_DIR`, else `$XDG_CONFIG_HOME/monster-cyberspace/`, else `~/.config/monster-cyberspace/` (directory 0700, files 0600), written atomically (tmp, fsync, rename); namespaces shared across `Preferences` in the process |
| `FS.h` | `fs::FS` (`open`, `exists`, `mkdir`, `remove`, `rmdir`), `File` (`openNextFile`, `read`, `write`, `size`, `name`, `path`, `isDirectory`, `close`) | POSIX files under a root directory; `..` refused |
| `esp_http_server.h` | the 25 `httpd_*` calls `web_server.cpp` makes (§7), added in Task 7 | a small POSIX-socket server, one thread |
| `ESPmDNS.h`, `WiFi.h` | not shimmed: their only user, `wifi_mgr.cpp`, is replaced | — |

`Preferences` files hold the same keys as NVS, so backup YAML, settings and
`settings_model.h` are unchanged. The token and passwords sit in the 0600
file, just as they sit unencrypted in NVS today. They are never logged.

### 4.2 Replacements — `linux/replace/`

The CMake build compiles these instead of the `src/` file of the same name:

| `src/` file | Linux version |
| --- | --- |
| `net/https_client.cpp` | libcurl easy handle, kept alive between requests; system CA store; same `HttpResult` and body cap |
| `net/sse_client.cpp` | libcurl on its own `std::thread`, feeding the same `SseParser`; `stop()` aborts through the progress callback and joins (2 s cap) |
| `net/tls_alloc.cpp` | no-op (`usePsram`), `peakBytes` 0 |
| `net/wifi_mgr.cpp` | read-only: `mode()` always Station; `connected()` true when a non-loopback interface has IPv4 and a default route; `ssid()`/`rssiBars()` from wireless extensions / `/proc/net/wireless`; `ip()` from `getifaddrs`; scan, setup AP and `setNetworks` do nothing |
| `app/time_service.cpp` | the system clock. Applies the zone (`setenv("TZ")`, `tzset`) as today; the snapshot lines say "system" for the source. No SNTP, GPS or RTC code |
| `app/restart.cpp` | after the delay, `execv("/proc/self/exe", argv)` (a fresh start, as a reboot would give) |
| `store/nvs_migrate.cpp` | no-op |
| `store/mascot_store.cpp` | the same encoded blob as the partition, in `~/.config/monster-cyberspace/mascot.bin`; still posts `EvtType::MascotChanged` |
| `sys/mem.cpp` | `malloc`/`calloc` |
| `src/main.cpp` | a `main()` that runs today's `setup()` steps (minus `nvsmigrate` and OTA), then waits for the quit flag (§5), stops the workers, and returns 0 |

`app/ota_service.cpp` and `app/power_service.cpp` are not built
(`RC_FEAT_OTA=0`, and `sleepsOnBattery()` is false).

### 4.3 Dependencies

Device (Debian trixie arm64 packages): `libcurl4` (OpenSSL),
`libasound2`, `libxkbcommon0` (only if §5's probe says we need it). The
`.deb` lists them in `Depends`. LVGL 9.5.0, ArduinoJson 7.4.3 and QRCode
0.0.1 are pinned the same as in `platformio.ini` and fetched by CMake
(`FetchContent`, exact tags). Simulator: SDL2 and libcurl from the host.
Fonts stay the compiled-in Terminus cells, so FreeType is not needed.

## 5. Device HAL — `src/hal/cardputer_zero/`

- **`name()`** "CardputerZero"; **`otaSlug()`** "cardputer-zero".
- **Display:** opens `/dev/fb0`, reads `FBIOGET_VSCREENINFO`, and `mmap`s it.
  `panelWidth/Height` are 320×170 (taken from the framebuffer, checked
  against those numbers). `pushRows` converts lvui's RGB565 frame rows into
  the framebuffer's format: 16 bpp is copied, 32 bpp (XRGB8888, the Template's
  `LV_COLOR_DEPTH 32`) is expanded. lvui keeps drawing a whole RGB565 frame
  in direct mode, as on PSRAM boards, so nothing in lvui changes.
- **No status bar of the launcher's:** the app owns the whole 320×170.
- **Keyboard:** a reader thread opens the evdev node (path from
  `MONSTER_KEYBOARD_DEVICE`, else the launcher's default, else the first
  `/dev/input/event*` with `KEY_A`). It does **not** `EVIOCGRAB` it: the
  launcher has to see a long Esc. Events go through a portable keymap
  `lib/core/src/input/keymap_cardputer_zero.{h,cpp}` (native tests) that
  turns evdev code + press/release/repeat + modifier state into
  `rc::KeyEvent`s:
  - letters, digits, punctuation and the `sym` layer to text (UTF-32);
    shifted forms per the printed legends;
  - `fn+F/X/Z/C` (or `KEY_UP/DOWN/LEFT/RIGHT` if the kernel already maps
    them) to arrows; `fn+K/N/L/M` to Home/End/PgUp/PgDn; `fn+1…0`, `fn+O/P`
    to F1…F12, giving the F10 help and the rest of `rc::kFunctionKeys`;
  - Enter/OK to Select; Tab to Tab (the entry menu); Backspace erases, and
    Back when nothing takes it (`backspaceIsBack()`);
  - **Esc short:** Back. **Esc held ≥ 1 s:** sets the quit flag, so the app
    returns to the launcher (§2.1);
  - `fn+U/I` step the backlight, `fn+S/D` the beep volume, `fn+H` the F10
    help.
  If the probe (§11) shows that raw evdev codes do not carry the `sym` layer
  or latched modifiers, the reader uses libxkbcommon with the system keymap,
  as the launcher does, and the keymap takes keysyms instead of codes. The
  keymap's tests cover whichever input form is chosen.
- **Flags:** `hasKeyboard`, `hasKeys`, `backspaceIsBack`, `anyInputWakes`
  true; no touch, rotation, GPS, haptic, notification light or SD slot
  (§8 covers backups); `canType` true; `fixedTextSize` false. 320×170 is
  53×14 cells in Compact 6×12. Compact and Comfortable are offered;
  `hasLargeText()` is false, as on the Wio L2's larger 320×240.
- **Battery:** `batteryPercent` from `capacity`, `externalPower` from
  `status` ("Charging"/"Full") of the first battery-type power supply; -1 /
  false when absent.
- **Backlight:** `backlight`/`backlightLevel` write the backlight sysfs
  `brightness` (scaled to `max_brightness`). The screensaver and screen-off
  keep working. If there is no sysfs node, screen-off paints black and
  `backlight` is a no-op.
- **Sound:** `beep` writes a short square wave to ALSA's default PCM on its
  own thread; `setVolume` scales it. `hasSound` and `hasVolume` are true.
- **Clock:** `rtcRead/rtcWrite` return false (Linux owns the RTC).

### 5.1 Simulator HAL — `src/hal/linux_sim/`

The same flags as the device, so the UI is identical. An SDL2 window shows
the 320×170 frame at 3× (`MONSTER_SIM_SCALE` to change it). SDL key events
are converted into the same evdev codes and fed through
`keymap_cardputer_zero`, with host keys standing in for the device's
modifiers: Left Alt = `fn`, Right Alt = `sym`. The keymap therefore gets
tested by hand on the desktop too. Battery reads 80 %. `beep` is silent, and
`MONSTER_SIM_BEEP=1` logs it. Closing the window (or SIGTERM) sets the quit flag.
`MONSTER_SIM_SHOT=<file.bmp>` writes the last frame on exit, for checks that
can't see the window. Both HALs build `flags_cardputer_zero.cpp`, so the flags
can't drift, and both implement `linuxhal::runMainLoop` (`linux/replace/linux_hal.h`):
macOS lets only the main thread touch SDL, so the simulator presents frames there.
**Agents run the simulator only as the demo build** (`-DRC_FAKE_API=1`). That
build has no network and is the only way an agent may exercise the UI (the
hard rule against calling the live API).

## 6. Changes inside `src/` and `lib/core`

Kept small, each behind a flag that every ESP env leaves at today's value:

- **`RC_FEAT_WIFI_MGMT`** (new in `feature_flags.h`, default 1; 0 on
  Linux). It compiles out the setup AP, the Wi-Fi screen and network editing
  in Settings, the Wi-Fi LED colour rows, the setup flow's Wi-Fi step and
  web config's Wi-Fi section. Setup at first boot then goes straight to
  sign-in. The Settings "wi-fi" status line stays: it shows the SSID and IP
  that Linux reports.
- **`board::webConfigOptIn()`** (new, default false; true on Linux). When it
  is true, web config does not start at boot. Settings gets a "Web config
  page" item (off by default, NVS `webcfg`, backup `webConfig`). It can only
  be turned on once a page password is set, and it shows `http://<ip>:8080`
  and a QR code. Turning it off stops the server.
- **`RC_LINUX` guards** only where a shim would be misleading: `main.cpp`
  (compiled out; the replacement has `main()`), the `[mem]` lines (they
  print RSS instead of internal heap), and Settings' "Turn setup AP on" and
  "Restart" wording ("Restart app").
- **Quit:** a process-wide `rc::requestQuit()` / `rc::quitRequested()` in
  `src/app/app_state` (no-ops on ESP). The UI task checks it each frame and
  the Linux `main()` waits on it.
- `scripts/release.sh`, CI and the flasher are covered in §9.

Nothing else in `src/screens`, `src/app` or `lib/` changes.

## 7. Web config on Linux

The same `web_server.cpp`, routes and page, through the `esp_http_server`
shim. It listens on **TCP 8080** (not 80, which needs root) on all IPv4
interfaces, and runs only while Settings' "Web config page" is on. Every
route already requires the page password (Basic auth). On Linux the server
also refuses to start without one, so it is never open. No captive portal or
mDNS. Backup download and upload, the mascot editor, and Settings work as
today. The Wi-Fi section and the X4-only compose box stay hidden.

## 8. Files, backups and settings

- Settings: `~/.config/monster-cyberspace/<namespace>` (the `Preferences`
  shim), mode 0600, directory 0700.
- Mascot: `~/.config/monster-cyberspace/mascot.bin`.
- Backups: `hasSdSlot()` is true and `sdMount()` returns an `fs::FS` rooted at
  `~/monster-cyberspace/` (created on first use, 0700), so backups land in
  `~/monster-cyberspace/backups/` as they land in `/backups/` on SD today.
  The user can copy them off over SSH, ADB or the web page. The Settings
  wording says "Save backup" without "to SD" on this board, using a new
  `board::backupPlaceName()` that defaults to "SD card".
- Factory reset deletes the config directory's files (not backups) and
  restarts the app.

## 9. Build, packaging, release

- **Device build:** `cmake --preset cp0 && cmake --build --preset cp0` in a
  Debian trixie **arm64** container (`linux/docker/Dockerfile`, pinned
  digest). Apple Silicon and GitHub's `ubuntu-24.04-arm` runners run it
  natively, without a cross toolchain or M5's 435 MB BSP sysroot, and its
  glibc matches the device's OS. Wrapper: `scripts/build-cardputer-zero.sh`
  (`--sim` for the simulator, `--deb` to package). On the user's machine the
  build tree stays on the external SSD, inside the project.
- **Simulator build:** `cmake --preset sim` on the host (macOS: SDL2 and curl
  from MacPorts or Homebrew).
- **Package:** CPack DEB,
  `monster-cyberspace-cardputer-zero-<version>_arm64.deb` (release naming as
  the other boards), package name `monster-cyberspace`, installing
  `/usr/bin/monster-cyberspace`,
  `/usr/share/APPLaunch/applications/monster-cyberspace.desktop` and the
  icon under `/usr/share/APPLaunch/share/images/`. No systemd unit and no
  conffiles. The version comes from the same source as `tools/set_app_version.py`.
- **Release:** `scripts/release.sh` gains a `LINUX_TARGETS` list. It builds
  the `.deb` and attaches it, but writes no OTA manifest (the OTA check is
  ESP-only), and the flasher doesn't list the board, since it can't flash a
  Linux device. The site gets a short "CardputerZero: download the .deb"
  note. Release versions are still the user's choice.
- **CI:** a `cardputer-zero` job on `ubuntu-24.04-arm` runs the device build
  and the package step, and uploads the `.deb` as an artifact; a second job
  builds the simulator on `ubuntu-latest`. The PlatformIO jobs are unchanged.
- **Install:** copy the `.deb` to the device (SSH/ADB) and run
  `sudo apt install ./monster-cyberspace-…_arm64.deb`.
  `docs/USE.md` and `docs/BUILD.md` describe this.

## 10. Testing

- `pio test -e native`: new `test_keymap_cardputer_zero` (every legend, the
  fn layer, sym, shift, latched modifiers, short and long Esc, unknown
  codes giving no key) and tests for any new `lib/core` logic.
- `pio test -e native_lv` (and lean): unchanged, must stay green.
- `linux/` has its own small CTest suite for the shims: `Preferences` round
  trip and atomic write, the `FS` directory walk, the `esp_http_server` shim
  against a local socket, and the FreeRTOS semaphore timeouts. Run on the
  host, no network beyond loopback.
- Every PlatformIO env must still build (`scripts/build-upload-monitor.sh -B`)
  with no size change beyond the new flag's defaults.
- **Simulator walk-through** (agent-safe, `RC_FAKE_API`): splash, Home, a
  room, C-Mail, Feed, Settings, F10 help, long Esc quits; screenshots into
  `docs/screenshots/cardputer-zero/` if wanted.
- **Hardware check** (a human, added to `docs/TESTING.md` under "Open
  hardware checks"): §11's probes, install, launcher entry, full screen,
  every key, long Esc to home, sign-in and the live checklist, beep,
  battery, backlight, web config opt-in, backup save and restore, relaunch
  keeps settings.

## 11. Open questions for the first device session

These are answered on the device by a human (or by the agent over SSH, if
the user allows it), then written back here before the HAL tasks start:

1. `fbset -i` / `cat /sys/class/graphics/fb0/{bits_per_pixel,virtual_size}`:
   the framebuffer's depth and size, and that a normal user can open it.
2. `evtest` on the keyboard node: what each key, `shift`, `sym` and `fn`
   send, and whether latched modifiers show up as events. This decides raw
   codes or xkbcommon (§5).
3. `ls /sys/class/backlight /sys/class/power_supply`: names and ranges.
4. `aplay -l`: the default PCM plays through the speaker.
5. `cat /etc/os-release`, `ldd --version`: confirms trixie/glibc for the
   container base.
6. While the app runs, does the launcher still act on a long Esc, and does
   it kill or wait?

## 12. Out of scope

- The CardputerZero Store (`app-builder.json`, store metadata, screenshots).
  Planned once the port has passed its hardware check.
- OTA. Updates are a new `.deb`.
- Cap modules (LoRa/GPS, CC1101), camera, IMU, IR, HDMI.
- Managing Wi-Fi from the app; the OS does it.
- Other Linux handhelds. The `linux/` layer is written so another one could
  follow, but none is planned.
- The long-term LoRa gateway idea (still needs the operator's permission).
