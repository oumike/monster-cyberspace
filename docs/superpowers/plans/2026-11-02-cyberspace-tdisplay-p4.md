# LilyGO T-Display P4 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A firmware env `tdisplay_p4` for the LilyGO T-Display P4 (AMOLED) that works in portrait, landscape, and landscape with the keyboard expansion, plus tap and alert vibration settings.

**Architecture:** Portable maths (2x scale and rotation, the keyboard map, the tap-buzz rule) lives in `lib/core` with native tests. The HAL in `src/hal/tdisplay_p4/` ports wadamesh's `variants/tdisplay_p4/` drivers (esp_lcd DSI, XL9535, GT9895, P4I2c1, keyboard, AW86224) to `src/hal/board.h`. App changes are three new board calls with weak defaults and two settings.

**Tech Stack:** pioarduino 55.03.312 (Arduino 3.3.12 / ESP-IDF 5.5), ESP32-P4 (`esp32p4_es`), esp_lcd MIPI-DSI, ESP-Hosted Wi-Fi on the C6, Unity native tests.

**Spec:** `docs/superpowers/specs/2026-11-02-cyberspace-tdisplay-p4-design.md`

Sources (read, don't copy blindly): wadamesh at `../../wadamesh/variants/tdisplay_p4/`
(`Xl9535.*`, `RM69A10Display.*`, `esp_lcd_rm69a10.*`, `Hi8561Touch.cpp` GT9895 path,
`P4I2c1.*`, `P4Keyboard.*`, `P4Audio.*`, `P4Haptic.*`, `P4HapticWaveform.h`, `target.cpp`
full-restart code); camillia-mt at `../../camillia/camillia-mt/` (`boards/tdisplay_p4.json`,
`src/hal/hw_tdisplay_p4.h` pins, `src/hal/tdisplay_p4_io.cpp`). Both are GPL-3.0; this repo's
licence must allow it (check `LICENSE` before Task 5 and keep each ported file's SPDX line).

## Global Constraints

- Never call the live Cyberspace API from tests or scripts (AGENTS.md hard rule).
- Never flash or erase a device. Building is fine.
- Commit on `main` once per task; never push. Commit messages end with
  `Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>`.
- Hardware access only through `src/hal/board.h`; nothing outside `src/hal/` includes `src/hal/tdisplay_p4/` headers.
- Every HAL file is wrapped in `#if defined(BOARD_TDISPLAY_P4)`.
- Board name `TDisplayP4`; OTA slug `tdisplay-p4`; env `tdisplay_p4`; script flag `-4` / `--tdisplay-p4`.
- Logical panel 284x616 (portrait), drawn 2x into the 568x1232 RM69A10.
- DSI: 2 lanes, 1000 Mbps, DPI 60 MHz, RGB565, HSYNC 50 HBP 150 HFP 50, VSYNC 40 VBP 120 VFP 80; LDO channel 3 at 1830 mV.
- The keyboard is detected once, in `board::begin()`; never re-probed.
- First boot with no saved rotation: Portrait (1) on this board, 0 everywhere else.
- Tap buzz 15 ms, alert buzz 120 ms. Settings `tapVibrate`, `alertVibrate` (default true), NVS `tapVib`, `alertVib`, backup keys `tapVibrate`, `alertVibrate`.

## Review Focus

- Keyboard half-present at boot (XL9555 answers, TCA8418 doesn't): the board must stay touch-only and rotatable — test in Task 7 (`detect` needs both).
- A backup without `tapVibrate`/`alertVibrate` restored onto the device: current values kept — test in Task 3.
- Touch at or past the raw edges (x ≥ 1060, y ≥ 2400, negatives): clamped onto the panel, never out of the frame — test in Task 1.
- A beep or buzz while the keyboard holds Wire1: the sound waits up to 50 ms then is dropped, never deadlocks the UI — enforced by `p4_i2c1::acquire` timeout in Task 7, exercised on hardware.
- Web config posting `tapVibrate`/`alertVibrate` on a board without haptics: ignored, not saved — check in Task 4 step list.

---

### Task 1: 2x scale and rotation maths (`ui/scale2x`)

**Files:**
- Create: `lib/core/src/ui/scale2x.h`, `lib/core/src/ui/scale2x.cpp`
- Test: `test/test_scale2x/test_main.cpp`

**Interfaces:**
- Produces (namespace `rc::scale2x`):
  - `struct Pt { int x, y; };`
  - `void frameSize(int nw, int nh, uint8_t r, int& w, int& h);` — r odd: nw x nh; even: nh x nw.
  - `Pt frameToNative(int x, int y, int nw, int nh, uint8_t r);`
  - `Pt nativeToFrame(int nx, int ny, int nw, int nh, uint8_t r);` — exact inverse.
  - `void nativeRows(int y0, int y1, int nw, int nh, uint8_t r, int& ny0, int& ny1);` — native rows `[ny0, ny1)` that frame rows `[y0, y1)` touch.
  - `Pt rawToNative(int rx, int ry, int rawW, int rawH, int nw, int nh);` — clamps raw to `[0, rawW-1]`/`[0, rawH-1]`, then `n = raw * nw / rawW`.
- Mapping (r in quarter turns, `ui/rotation.h`; native is portrait):
  - r=1: `n = (x, y)`; r=3: `n = (nw-1-x, nh-1-y)`;
  - r=0: `n = (nw-1-y, x)`; r=2: `n = (y, nh-1-x)`.

- [x] **Step 1: Write the failing tests** (nw=284, nh=616)

```cpp
void test_frame_size() { int w, h; frameSize(284, 616, 1, w, h); TEST_ASSERT_EQUAL(284, w); TEST_ASSERT_EQUAL(616, h);
                         frameSize(284, 616, 0, w, h); TEST_ASSERT_EQUAL(616, w); TEST_ASSERT_EQUAL(284, h); }
void test_portrait_identity() { expectPt(frameToNative(0, 0, 284, 616, 1), 0, 0); expectPt(frameToNative(283, 615, 284, 616, 1), 283, 615); }
void test_portrait_180() { expectPt(frameToNative(0, 0, 284, 616, 3), 283, 615); }
void test_landscape() { expectPt(frameToNative(0, 0, 284, 616, 0), 283, 0); expectPt(frameToNative(615, 283, 284, 616, 0), 0, 615);
                        expectPt(frameToNative(615, 0, 284, 616, 0), 283, 615); }
void test_landscape_180() { expectPt(frameToNative(0, 0, 284, 616, 2), 0, 615); expectPt(frameToNative(615, 283, 284, 616, 2), 283, 0); }
void test_round_trip() { // every r, points (0,0) (w-1,h-1) (w-1,0) (0,h-1) (37,101): nativeToFrame(frameToNative(p)) == p
}
void test_native_rows() { int a, b;
  nativeRows(10, 20, 284, 616, 1, a, b); TEST_ASSERT_EQUAL(10, a); TEST_ASSERT_EQUAL(20, b);
  nativeRows(0, 1, 284, 616, 3, a, b);   TEST_ASSERT_EQUAL(615, a); TEST_ASSERT_EQUAL(616, b);
  nativeRows(5, 6, 284, 616, 0, a, b);   TEST_ASSERT_EQUAL(0, a); TEST_ASSERT_EQUAL(616, b); }
void test_raw_to_native() {
  expectPt(rawToNative(0, 0, 1060, 2400, 284, 616), 0, 0);
  expectPt(rawToNative(1059, 2399, 1060, 2400, 284, 616), 283, 615);
  expectPt(rawToNative(530, 1200, 1060, 2400, 284, 616), 142, 308);
  expectPt(rawToNative(-5, 3000, 1060, 2400, 284, 616), 0, 615);  // Review Focus: edges clamp
}
```

- [x] **Step 2: Run, expect a compile failure** — `pio test -e native -f test_scale2x`
- [x] **Step 3: Implement `lib/core/src/ui/scale2x.{h,cpp}`** with the mapping above.
- [x] **Step 4: Run, expect PASS** — `pio test -e native -f test_scale2x`
- [x] **Step 5: Commit** — `git commit -m "scale2x: 2x panel scale and rotation maths"`

### Task 2: Keyboard map (`input/keymap_tdisplay_p4`)

**Files:**
- Create: `lib/core/src/input/keymap_tdisplay_p4.h`, `.cpp`
- Test: `test/test_tdisplay_p4_input/test_main.cpp`

**Interfaces:**
- Produces (namespace `rc`):
  - `constexpr uint32_t kP4ModifierMs = 1500, kP4RepeatDelayMs = 450, kP4RepeatMs = 70;`
  - `class TdisplayP4Keys { public: KeyEvent key(uint8_t keyNum, bool pressed, uint32_t ms); KeyEvent poll(uint32_t ms); bool capsLock() const; bool takeLightStep(); };`
- Key numbers are the TCA8418's (1-based, `row*10+col+1`), table from wadamesh `P4Keyboard.cpp` `kMap` (base, shift, sym). Specials: 11, 12 Esc → `Back`; 31 Caps Lock (toggles); 41 Alt (ignored); 51, 58 Sym (one-shot sym layer); 53 Shift (one-shot); 50 Up, 60 Down, 59 Left, 68 Right; 54 Tab → `Palette`; 55–57 space; 63 Backspace (repeats); 64, 66 Enter → `Select`; 65 → `Emoji`; 61 (F11) → `None` and sets the light-step flag; 1–10 (F1–F10) → `None`. Up/Down/Left/Right repeat like Backspace. Releases return `None` except they stop a repeat.

- [x] **Step 1: Write the failing tests**

```cpp
void test_letters()   { TdisplayP4Keys k; expectChar(k.key(21, true, 0), 'q'); expectChar(k.key(32, true, 0), 'a'); expectChar(k.key(48, true, 0), 'm'); expectChar(k.key(56, true, 0), ' '); }
void test_digits()    { TdisplayP4Keys k; expectChar(k.key(13, true, 0), '1'); expectChar(k.key(62, true, 0), '9'); expectChar(k.key(67, true, 0), '0'); }
void test_shift_one_shot() { TdisplayP4Keys k; k.key(53, true, 0); expectChar(k.key(21, true, 10), 'Q'); expectChar(k.key(21, true, 20), 'q'); }
void test_shift_lapses()   { TdisplayP4Keys k; k.key(53, true, 0); expectChar(k.key(21, true, 1501), 'q'); }
void test_sym()       { TdisplayP4Keys k; k.key(51, true, 0); expectChar(k.key(39, true, 10), '/'); k.key(58, true, 20); expectChar(k.key(40, true, 30), '?'); }
void test_sym_falls_back_to_base() { TdisplayP4Keys k; k.key(51, true, 0); expectChar(k.key(42, true, 10), 'z'); }
void test_caps_lock() { TdisplayP4Keys k; k.key(31, true, 0); TEST_ASSERT_TRUE(k.capsLock()); expectChar(k.key(21, true, 10), 'Q'); expectChar(k.key(21, true, 20), 'Q');
                        k.key(31, true, 30); TEST_ASSERT_FALSE(k.capsLock()); expectChar(k.key(21, true, 40), 'q'); }
void test_actions()   { TdisplayP4Keys k; expectAction(k.key(11, true, 0), Action::Back); expectAction(k.key(64, true, 0), Action::Select);
                        expectAction(k.key(54, true, 0), Action::Palette); expectAction(k.key(65, true, 0), Action::Emoji);
                        expectAction(k.key(50, true, 0), Action::Up); expectAction(k.key(60, true, 0), Action::Down);
                        expectAction(k.key(59, true, 0), Action::Left); expectAction(k.key(68, true, 0), Action::Right); }
void test_backspace_repeat() { TdisplayP4Keys k; expectAction(k.key(63, true, 0), Action::Backspace);
                        expectAction(k.poll(449), Action::None); expectAction(k.poll(450), Action::Backspace);
                        expectAction(k.poll(519), Action::None); expectAction(k.poll(520), Action::Backspace);
                        k.key(63, false, 530); expectAction(k.poll(600), Action::None); }
void test_arrow_repeat() { TdisplayP4Keys k; k.key(60, true, 0); expectAction(k.poll(450), Action::Down); }
void test_f11_light() { TdisplayP4Keys k; expectAction(k.key(61, true, 0), Action::None); TEST_ASSERT_TRUE(k.takeLightStep()); TEST_ASSERT_FALSE(k.takeLightStep()); }
void test_ignored()   { TdisplayP4Keys k; expectAction(k.key(21, false, 0), Action::None); expectAction(k.key(0, true, 0), Action::None);
                        expectAction(k.key(69, true, 0), Action::None); expectAction(k.key(41, true, 0), Action::None); expectAction(k.key(1, true, 0), Action::None); }
```

- [x] **Step 2: Run, expect a compile failure** — `pio test -e native -f test_tdisplay_p4_input`
- [x] **Step 3: Implement `TdisplayP4Keys`** (model on `keymap_pager.cpp`).
- [x] **Step 4: Run, expect PASS**
- [x] **Step 5: Commit** — `git commit -m "T-Display P4: keyboard map"`

### Task 3: Vibration settings and the tap-buzz rule (portable)

**Files:**
- Modify: `lib/core/src/config/settings_model.h` (add `bool tapVibrate = true, alertVibrate = true;` after `buzzer`)
- Modify: `lib/core/src/config/backup_yaml.cpp` (the `Bool` staging struct at line ~69, the `isBool` key list ~290, the `Bool&` selector ~303, the writer ~413, the apply ~550)
- Create: `lib/core/src/ui/haptics.h`, `.cpp`
- Test: `test/test_backup_yaml/test_main.cpp` (extend), `test/test_touch_input/test_main.cpp` (extend)

**Interfaces:**
- Produces: `rc::Settings::tapVibrate`, `rc::Settings::alertVibrate`;
  `namespace rc { constexpr int kTapBuzzMs = 15, kAlertBuzzMs = 120; bool tapBuzz(const Gesture& g, const TouchOutput& out); }` — true when `g.kind == GestureKind::Tap` and (`!out.keys.empty()` or `out.oskKey >= 0`).
- Backup writes `tapVibrate: true|false` and `alertVibrate: true|false` after `buzzer:`.

- [x] **Step 1: Write the failing tests**

```cpp
// test_backup_yaml: round trip — set both false, write, parse, TEST_ASSERT_FALSE for both.
// test_backup_yaml: "a file without them keeps current values" — Settings s with tapVibrate=false, alertVibrate=true;
//   apply a backup text that has buzzer but neither vibrate key; TEST_ASSERT_FALSE(s.tapVibrate); TEST_ASSERT_TRUE(s.alertVibrate).
// test_touch_input:
void test_tap_buzz() {
  Gesture tap; tap.kind = GestureKind::Tap; TouchOutput none, hit, osk; hit.keys.push_back(KeyEvent{Action::Select}); osk.oskKey = 3;
  TEST_ASSERT_FALSE(tapBuzz(tap, none)); TEST_ASSERT_TRUE(tapBuzz(tap, hit)); TEST_ASSERT_TRUE(tapBuzz(tap, osk));
  Gesture drag; drag.kind = GestureKind::Drag; TEST_ASSERT_FALSE(tapBuzz(drag, hit));
}
```

- [x] **Step 2: Run, expect failures** — `pio test -e native -f test_backup_yaml -f test_touch_input`
- [x] **Step 3: Implement** the fields, the backup keys and `tapBuzz`.
- [x] **Step 4: Run the whole native suite, expect PASS** — `pio test -e native`
- [x] **Step 5: Commit** — `git commit -m "Tap and alert vibration settings, backup keys"`

### Task 4: App wiring — default rotation, haptics, Settings and web config

**Files:**
- Modify: `src/hal/board.h` (declare the three calls below with comments in the file's style)
- Modify: `src/hal/board_defaults.cpp` (weak `defaultRotation() { return 0; }`, `hasHaptic() { return false; }`, `haptic(int) {}`)
- Modify: `src/store/settings.cpp`, `src/store/settings.h` (`rot` default `board::defaultRotation()` at line ~76; load/save `tapVib`, `alertVib`; `void saveVibrate(bool tap, bool alert);`; include them in the save-all path at ~258)
- Modify: `src/screens/settings.cpp` (items `kTapVibrate`, `kAlertVibrate` after `kKbLight`, only `if (board::hasHaptic())`; toggled and listed like `kSpellCheck`: labels "Tap vibration" / "Alert vibration", values "on"/"off")
- Modify: `src/web/web_server.cpp` (GET: `if (board::hasHaptic()) { d["tapVibrate"]=…; d["alertVibrate"]=…; }`; POST: read like `spellCheck`, save only when `board::hasHaptic()` and changed, through `settings::saveVibrate`)
- Modify: `src/web/page.h` (two checkboxes `tapvib`, `alertvib` in a row hidden unless the GET has `tapVibrate`; sent in `savePrefs`)
- Modify: `src/app/ui_task.cpp` (after `resolver.resolve` ~800: `if (app().cfg.tapVibrate && board::hasHaptic() && tapBuzz(g, out)) board::haptic(kTapBuzzMs);`; beside each alert beep at ~401, ~409, ~427: `if (app().cfg.alertVibrate && board::hasHaptic()) board::haptic(kAlertBuzzMs);` independent of `buzzer`)

**Interfaces:**
- Produces: `uint8_t board::defaultRotation();` `bool board::hasHaptic();` `void board::haptic(int ms);` (returns at once).
- Consumes: Task 3's fields, `tapBuzz`, `kTapBuzzMs`, `kAlertBuzzMs`.

- [x] **Step 1: Make the changes listed above.**
- [x] **Step 2: Native suite still green** — `pio test -e native`
- [x] **Step 3: Every existing env builds** — `scripts/build-upload-monitor.sh -B` (exit 0)
- [x] **Step 4: Commit** — `git commit -m "Board default rotation and haptics hooks; vibration in Settings and web config"`

### Task 5: Env, expander, display and the board skeleton

**Files:**
- Create: `boards/tdisplay_p4.json` (camillia-mt's, unchanged; PlatformIO reads `boards/` by default)
- Modify: `platformio.ini` (new `[env:tdisplay_p4]`, list the board in the header comment)
- Create: `src/hal/tdisplay_p4/pins_tdisplay_p4.h` (spec §2 table, constants in `namespace tdp4`)
- Create: `src/hal/tdisplay_p4/io_xl9535.{h,cpp}` (from wadamesh `Xl9535`: `begin()`, `powerOnSequence()`, `set(bit, level)`, on `Wire` 7/8)
- Create: `src/hal/tdisplay_p4/esp_lcd_rm69a10.{h,cpp}` (wadamesh, verbatim)
- Create: `src/hal/tdisplay_p4/panel_rm69a10.{h,cpp}` (`bool begin(); uint16_t* frame(); void flush(int ny0, int ny1); void brightness(uint8_t); void sleep(bool);`)
- Create: `src/hal/tdisplay_p4/board_tdisplay_p4.cpp`

**Interfaces:**
- Consumes: Task 1's `scale2x`; Task 4's board calls.
- Produces: `tdp4::xl9535::set(uint8_t bit, bool high)`, `tdp4::panel::*` above, `board::*` for the display, rotation, backlight, side button, LoRa sleep and full restart. Later tasks fill `readTouch`, keyboard, sound, GPS/RTC/battery/SD in this file.

Env (exact values):

```ini
[env:tdisplay_p4]
board = tdisplay_p4
board_build.flash_size = 16MB
board_upload.flash_size = 16MB
board_upload.maximum_size = 16777216
board_build.partitions = partitions_16mb.csv
build_flags =
    -DBOARD_TDISPLAY_P4
    -DBOARD_HAS_PSRAM
    -DARDUINO_USB_MODE=1
    -DARDUINO_USB_CDC_ON_BOOT=1
    -std=gnu++17
build_unflags = -std=gnu++11
extra_scripts =
    pre:tools/set_app_version.py
    pre:tools/gen_spell_dict.py
    pre:tools/gen_mascot_presets.py
    post:tools/upload_erase.py
lib_deps =
    ${env.lib_deps}
    https://github.com/pschatzmann/arduino-audio-driver/archive/v0.3.0.zip
```

Board behaviour in this task:
- `name()` "TDisplayP4"; `otaSlug()` "tdisplay-p4"; `defaultRotation()` 1; `canRotate()` true (Task 7 makes it depend on the keyboard).
- `begin()`: `Wire.begin(7, 8, 400000)`, `xl9535::begin()` + `powerOnSequence()`, `panel::begin()`, LoRa sleep (SPI 2/3/4, CS 24; reset via XL9535 bit 14 low 10 ms then high; wait BUSY 6 low; send SX126x `SetSleep` 0x84 0x00), side button GPIO35 `INPUT_PULLUP`, install the full-restart shutdown handler (wadamesh `tdisplayP4InstallFullRestart`).
- A `uint8_t rotation` file static; `panelWidth/Height` from `scale2x::frameSize(284, 616, rotation, …)`.
- `pushRows(frame, y0, y1)`: for each frame pixel, `frameToNative` → write the 2x2 block into `panel::frame()` (568 wide); then `nativeRows` → `panel::flush(2*ny0, 2*ny1)` (cache write-back of those rows).
- `setRotation(r)`: if changed, clear the frame buffer and flush it all.
- `backlight(on)` / `backlightLevel(l)` → `panel::brightness` (on = 255); `displaySleep(s)` → `panel::sleep(s)`.
- `pollKey`: side button press (debounced 30 ms, on press) → `Action::ScreenOff`. `wakeHint()` "press the side button to wake".
- `hasTouch()` true (reads nothing until Task 6), `hasSdSlot()` true (null mount until Task 9), the rest return "none".

- [x] **Step 1: Make the changes above.**
- [x] **Step 2: Build** — `pio run -e tdisplay_p4`. Expected: SUCCESS. Fix P4-only compile errors in shared code with the smallest guard (e.g. S3-only headers); note each in the commit message.
- [x] **Step 3: Native suite still green** — `pio test -e native`
- [x] **Step 4: Commit** — `git commit -m "T-Display P4: env, XL9535, RM69A10 panel at 2x, board skeleton"`

### Task 6: GT9895 touch

**Files:**
- Create: `src/hal/tdisplay_p4/touch_gt9895.{h,cpp}` (from wadamesh `Hi8561Touch.cpp`'s GT9895 path: reset via XL9535 bit 3, read point on `Wire` 0x5D; `bool read(int16_t& rx, int16_t& ry);`)
- Modify: `src/hal/tdisplay_p4/board_tdisplay_p4.cpp`

**Interfaces:**
- Consumes: `scale2x::rawToNative(rx, ry, 1060, 2400, 284, 616)`, `scale2x::nativeToFrame(…, rotation)`.
- Produces: `board::readTouch(x, y)` in frame coordinates.

- [x] **Step 1: Implement and wire `readTouch`.** `doubleTapWakes()` false, `anyInputWakes()` false (Task 7 sets it with the keyboard).
- [x] **Step 2: Build** — `pio run -e tdisplay_p4` SUCCESS
- [x] **Step 3: Commit** — `git commit -m "T-Display P4: GT9895 touch"`

### Task 7: Shared Wire1 and the keyboard expansion

**Files:**
- Create: `src/hal/tdisplay_p4/i2c1_tdisplay_p4.{h,cpp}` (wadamesh `P4I2c1`: `enum Route { Keyboard, Audio }; TwoWire* acquire(Route, uint32_t waitMs); void release();` — FreeRTOS mutex; re-pins Wire1 to 46/45 or 20/21)
- Create: `src/hal/tdisplay_p4/keyboard_tdisplay_p4.{h,cpp}` (`bool detect();` `void poll();` `bool next(uint8_t& keyNum, bool& pressed);` `void light(uint8_t duty, bool screenOn);` `void leds(bool on);`)
- Modify: `src/hal/tdisplay_p4/board_tdisplay_p4.cpp`

**Interfaces:**
- Consumes: Task 2's `TdisplayP4Keys`.
- Produces: `tdp4::i2c1::acquire/release` (Task 8 uses `Route::Audio` with `waitMs` 50).

Behaviour:
- `detect()` (once, in `begin()`): XL9555 at 0x20 reads both config registers, TCA8418 reset low 10 ms / high via XL9555 bit 6, then TCA8418 at 0x34 answers and is configured (7x10 matrix, as wadamesh). Both must answer; otherwise false.
- With the keyboard: rotation fixed at 0, `canRotate()` false, `setRotation` ignored, `hasKeyboard()`, `hasKeys()`, `backspaceIsBack()`, `anyInputWakes()`, `hasKbLight()` true; `kbLightDefault()` = Medium's index; `kbLight(duty, screenOn)` → PWM on GPIO47 (20 kHz), dark when `!screenOn`.
- `pollKey`: `keyboard::poll()`, each event through `TdisplayP4Keys::key`, then `TdisplayP4Keys::poll(millis())`; after each key, `leds(keys.capsLock())`; on `takeLightStep()` step `app` kb light — post the next `KbLight` through the existing kb-light path (find how the T-Deck Pro or Pager steps it; reuse it).
- Without it: everything as Task 5/6.

- [x] **Step 1: Implement.**
- [x] **Step 2: Build** — `pio run -e tdisplay_p4` SUCCESS
- [x] **Step 3: Commit** — `git commit -m "T-Display P4: shared Wire1, keyboard expansion found at boot"`

### Task 8: Speaker and haptics

**Files:**
- Modify: `src/hal/wio_l2/audio_es8311.{h,cpp}` (add `TwoWire* wire = nullptr;` to `Config`, null meaning `Wire`; use it for every codec transfer)
- Create: `src/hal/tdisplay_p4/haptic_aw86224.{h,cpp}` (wadamesh `P4Haptic` + `P4HapticWaveform.h`; `void begin(); void buzz(int ms);` — `buzz` queues; a task plays it)
- Modify: `src/hal/tdisplay_p4/board_tdisplay_p4.cpp`

**Interfaces:**
- Consumes: `tdp4::i2c1::acquire(Route::Audio, 50)` / `release()` as the es8311 `lock`/`unlock` (a failed acquire drops the tone).
- Produces: `board::beep`, `board::hasHaptic()` true, `board::haptic(ms)`.

- [x] **Step 1: Implement.** es8311 `Config{addr 0x18, mclk 13, bck 12, ws 9, dout 10, ampSettleMs 8, amp: XL9535 bit 6, lock/unlock as above, wire &Wire1}`.
- [x] **Step 2: Build** — `pio run -e tdisplay_p4 -e pager -e wio_l2 -e cardputer_adv` SUCCESS (the shared driver changed)
- [x] **Step 3: Commit** — `git commit -m "T-Display P4: ES8311 beeps, AW86224 haptics"`

### Task 9: GPS, RTC, battery, SD

**Files:**
- Create: `src/hal/tdisplay_p4/peripherals_tdisplay_p4.cpp` (GPS UART1 RX 22 / TX 23 at 9600 with XL9535 bit 9; PCF8563 on `Wire`; BQ27220 on `Wire`; SD_MMC 4-bit with XL9535 bit 13 low while mounted)
- Modify: `src/hal/tdisplay_p4/board_tdisplay_p4.cpp` (drop the Task 5 stubs)

**Interfaces:**
- Produces: `hasGps`, `gpsPower`, `gpsRead`, `rtcRead`, `rtcWrite`, `batteryPercent`, `batteryVolts`, `externalPower`, `hasSdSlot`, `sdMount`, `sdUnmount` per `board.h`. Model on the Pager's `gps_rtc_pager.cpp` and `sd_pager.cpp`.

- [x] **Step 1: Implement.**
- [x] **Step 2: Build** — `pio run -e tdisplay_p4` SUCCESS
- [x] **Step 3: Commit** — `git commit -m "T-Display P4: GPS, RTC, battery, SD"`

### Task 10: Tooling and docs

**Files:**
- Modify: `scripts/build-upload-monitor.sh` (`--tdisplay-p4|-4) BOARD=tdisplay_p4`, usage text, the `-B` env list)
- Modify: `scripts/release.sh` (env → `tdisplay-p4`, OTA)
- Modify: `.github/workflows/build.yml` (matrix)
- Modify: `site/index.html` (board entry, as the Cyber Deck Plus's)
- Modify: `AGENTS.md` (board list, spec/plan links, commands, layout paragraph, progress)
- Modify: `docs/BUILD.md` (env; the C6 ESP-Hosted 2.12.13 image and how to flash it, after camillia-mt's BUILD.md section; P4 OTA doesn't update the C6)
- Modify: `docs/USE.md` (the three ways to hold it; Tap/Alert vibration)
- Modify: `docs/TESTING.md` (open hardware checks, spec §5 list)

- [x] **Step 1: Make the changes.**
- [x] **Step 2: Verify** — `node --test site/*.test.mjs`, `pio test -e native`, `scripts/build-upload-monitor.sh -B` all pass
- [x] **Step 3: Tick this plan's boxes; commit** — `git commit -m "T-Display P4: scripts, release, flasher and docs"`
