# LilyGO T-Deck Plus Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Run Rhino Cyberspace on the LilyGO T-Deck Plus, with its keyboard, trackball, touch panel, GPS, battery, speaker and SD card working, and the M9 and Wio L2 unchanged.

**Architecture:**
- A new HAL in `src/hal/tdeck/` implements `src/hal/board.h`.
- The M9's ST7789 driver and the Wio's GT911 driver take a config struct, so the T-Deck reuses them.
- The board-specific input rules are pure functions in `lib/core/src/input/`, tested on the host: the key map, Backspace-as-Back, any-input wake, and trackball click timing.
- `ui_task` applies those rules behind three new HAL flags.

**Tech Stack:** C++17, Arduino-ESP32 3.3 (pioarduino 55.03.312), PlatformIO, Unity host tests. Libraries: `Wire`, `SPI`, `SD`, `ESP_I2S`.

**Spec:** `docs/superpowers/specs/2026-10-26-cyberspace-tdeck-plus-design.md`. Read it together with the touch plan (`docs/superpowers/plans/2026-10-26-cyberspace-touch.md`) and the backup plan (`docs/superpowers/plans/2026-10-26-cyberspace-backup.md`). This plan consumes interfaces from both.

**Spec amendments (2026-10-26, from the user):**
- **SD card.** Backup now runs on every hardware board, so the T-Deck's SD card moves into scope. It uses the backup HAL API: `hasSdSlot`, `sdMount` and `sdUnmount`, on SPI with CS 39.
- **External power.** `externalPower()` uses the Wio's rule: `HWCDC::isPlugged() || volts > 4.25`. The T-Deck's USB goes to the S3's own port, so a USB host is visible.

Task 7 writes both amendments into the spec.

## Global Constraints

**Order and builds**
- This plan starts only after the backup and touch plans are complete. It reuses:
  - `Screen::acceptsText`, `tileMove`, `Gestures`, `touchToFrame`, `wakeHint`, `canRotate` and `setRotation` from the touch plan;
  - `hasSdSlot`, `sdMount` and `sdUnmount` from the backup plan.
- **Build only. Never flash, erase or open a serial port.** The user is using the USB port for other work. Every hardware check goes into `docs/TESTING.md` for the user to run.
- Every task that touches `src/` builds every firmware env:
  - `scripts/build-upload-monitor.sh -B`, which covers four envs until Task 4 and six after it;
  - `pio test -e native` is green.

**What must not change**
- The M9 and Wio behave exactly as before.
- `hasKeyboard()` is true on the M9 and false on the Wio.
- `backspaceIsBack()` and `anyInputWakes()` are false on both.

**Hard rules**
- Never call the live Cyberspace API.
- Never log passwords, tokens or GPS positions.
- Every write comes from a key press.
- No captive portal.

**Libraries and layout**
- No LovyanGFX, LVGL or RadioLib in the T-Deck envs.
- lib/core stays board-agnostic: no Arduino, no board headers.

**Hardware values (spec §3)**
- **Power:** `POWERON` 10, driven HIGH first.
- **Panel:** SPI SCK 40 / MOSI 41 / MISO 38, CS 12, DC 11, RST −1, BL 42 (PWM 12 kHz). Inverted.
- **Bus chip selects:** LoRa CS 9 and SD CS 39 are driven HIGH before the panel starts.
- **I2C:** SDA 18 / SCL 8. Keyboard at 0x55, INT 46 (unused, polled every 10 ms). GT911 at 0x5D, INT 16, no reset line.
- **Trackball pins** (active low). Per camillia: Right → 2, Left → 1, Up → 15, Down → 3, Click → 0.
- **Trackball timing:**
  - one move event per 45 ms at most;
  - a click held 2000 ms gives `ScreenOff`;
  - a click within 160 ms of motion waits until the motion stops.
- **Backspace guard:** 400 ms.
- **GPS:** L76K on `Serial1`, RX 44 / TX 43, 38400 baud.
- **Battery:** ADC 4, ×2 divider, ×1.03 calibration.
- **Speaker:** I2S WS 5 / BCK 7 / DOUT 6, 16 kHz, 16-bit mono.
- **SD:** SPI, CS 39, 20 MHz.

**Names**
- `board::name()` returns `"TDeck"`.
- The setup AP is `Rhino-Cyberspace-TDeck-Setup`.
- `wakeHint()` returns `"press any key to wake"`.

## Review Focus

1. **A held or fast Backspace clears the Room input, then stays put.** It must not jump back to the room list. Task 2 `test_backspace_guard_*` pins this, and Task 3 wires `lastDeleteMs`.
2. **The keypress that wakes the screen must not type or select.** A trackball click, a letter or a touch while the screen is dark only wakes it. Task 2 pins `wakesScreen`; Task 3 swallows the waking touch until the finger lifts (`touchSwallow`).
3. **Rolling the trackball while clicking must not open the wrong tile.** Task 5 pins the deferral in `test_click_during_motion_*`.
4. **Holding the click to turn the screen off must not also fire Select on release.** Task 5 pins this in `test_hold_no_select_on_release`.
5. **Pulling the SD card mid-backup on the T-Deck must not hang the shared SPI bus or the panel.** Task 6 uses the backup plan's mount-per-operation API and holds LoRa CS high. `docs/TESTING.md` gets a hardware line for it.

---

## File map

| File | Responsibility |
|---|---|
| `src/hal/board.h` | Adds `hasKeyboard()`, `backspaceIsBack()`, `anyInputWakes()` (Task 1) |
| `src/hal/m9/panel_st7789.{h,cpp}` | Takes `st7789::Config` (Task 1) |
| `src/hal/wio_l2/touch_gt911.{h,cpp}` | Takes `gt911::Config` (Task 1) |
| `lib/core/src/input/keymap_tdeck.{h,cpp}` | `mapTdeckKey` (Task 2) |
| `lib/core/src/input/backspace.{h,cpp}` | `resolveBackspace` (Task 2) |
| `lib/core/src/input/key_event.{h,cpp}` | `wakesScreen(e, anyInputWakes)` (Task 2) |
| `lib/core/src/input/click_timer.{h,cpp}` | Trackball click timing (Task 5) |
| `lib/core/src/power/battery.{h,cpp}` | `batteryPercentFromVolts`, shared by all three boards (Task 6) |
| `src/screens/router.h` and the text screens | `consumesBackspace()` (Task 3) |
| `src/app/ui_task.cpp` | Backspace-as-Back and the any-input wake (Task 3) |
| `src/hal/tdeck/pins_tdeck.h`, `board_tdeck.cpp`, `keyboard_tdeck.{h,cpp}` | Board bring-up (Task 4) |
| `src/hal/tdeck/trackball_tdeck.{h,cpp}` | ISRs and move debounce (Task 5) |
| `src/hal/tdeck/gps_tdeck.cpp`, `audio_tdeck.{h,cpp}`, `sd_tdeck.cpp` | GPS, beep, SD (Task 6) |
| `platformio.ini`, `scripts/build-upload-monitor.sh`, `.github/workflows/build.yml` | `tdeck` and `tdeck_fake` envs (Task 4) |
| `test/test_tdeck_input/test_main.cpp` | Host tests for Tasks 2 and 5, plus the battery test (Task 6) |

---

### Task 1: Shared driver configs and the new HAL flags

**Files:**
- Modify: `src/hal/board.h`, `src/hal/m9/{board_m9,panel_st7789}.{h,cpp}`, `src/hal/wio_l2/{board_wio_l2,touch_gt911}.{h,cpp}`, `src/app/ui_task.cpp`

**Interfaces:**
- Produces, in `board.h`:
  ```cpp
  bool hasKeyboard();      // a physical keyboard: M9 true, Wio false
  bool backspaceIsBack();  // Backspace means Back when nothing consumes it: M9/Wio false
  bool anyInputWakes();    // any key or touch wakes a dark screen: M9/Wio false
  ```
- Produces, in `panel_st7789.h`:
  ```cpp
  struct Config { int sck, mosi, miso, cs, dc, rst; uint32_t hz; uint8_t madctl; bool invert; };
  void begin(const Config& c);  // rst < 0: no reset line, software reset (0x01) only
  ```
  The M9 passes `{.., m9::TFT_RST, m9::TFT_SPI_HZ, 0xA0, true}`, which are today's values. `pushRows` and `sleep` keep their signatures and use the stored config.
- Produces, in `touch_gt911.h`:
  ```cpp
  struct Config { TwoWire* bus; uint8_t addr; void (*lock)(); void (*unlock)(); };  // lock/unlock may be null
  bool begin(const Config& c);
  ```
  The Wio passes `{&Wire, <today's address>, …}`, with lock and unlock wrapping its `I2cGuard`. `read` keeps returning raw native coordinates; the touch plan's Task 11 moved the transform into `board::readTouch`.
- The touch plan's on-screen keyboard is shown only when `board::hasTouch() && !board::hasKeyboard()`. Add `!board::hasKeyboard()` to the Focus → show condition in `ui_task`.

- [x] **Step 1: Implement** the changes above. There is no host test, because this is wiring. Behaviour is pinned by the M9 and Wio values staying exactly as they are today.
- [x] **Step 2: Run** `pio test -e native` and `scripts/build-upload-monitor.sh -B`. Expected: green, and all four envs build.
- [x] **Step 3: Check that M9 and Wio behaviour is unchanged.**
  - `git diff` shows the same MADCTL (`0xA0`), invert, reset pulse and SPI rate on the M9.
  - The Wio shows the same GT911 address and lock.
  - Both boards return the flag values in Global Constraints.
- [x] **Step 4: Commit** `refactor(hal): st7789 and gt911 take a config; keyboard, Backspace and wake flags`.

### Task 2: The T-Deck key map, Backspace-as-Back and the any-input wake (lib/core)

**Files:**
- Create: `lib/core/src/input/keymap_tdeck.{h,cpp}`, `lib/core/src/input/backspace.{h,cpp}`, `test/test_tdeck_input/test_main.cpp`
- Modify: `lib/core/src/input/key_event.{h,cpp}`, `test/test_keymap_web/test_main.cpp`, `src/app/ui_task.cpp` (the call site: `wakesScreen(k, board::anyInputWakes())`)

**Interfaces:**
- Produces (namespace `rc`):
  ```cpp
  KeyEvent mapTdeckKey(uint8_t raw);  // 0x08 Backspace; 0x0D/0x0A Select; 0x20–0x7E Char; else None
  enum class BackspaceUse : uint8_t { Delete, Back, Drop };
  constexpr uint32_t kBackspaceGuardMs = 400;
  // consumes: the screen has text to delete. msSinceLastDelete: since the last
  // Backspace that deleted (UINT32_MAX when none).
  BackspaceUse resolveBackspace(bool consumes, uint32_t msSinceLastDelete);
  // anyInputWakes false: only ScreenOff (today). true: any action except None.
  bool wakesScreen(const KeyEvent& e, bool anyInputWakes);
  ```
  The one-argument `wakesScreen` is removed. Existing tests call `wakesScreen(e, false)`.

- [x] **Step 1: Write the failing tests** in `test/test_tdeck_input/test_main.cpp`:
  ```cpp
  void test_tdeck_printable()      // mapTdeckKey('a') == Char 'a'; ' ' == Char ' '; '~' == Char '~'; 'Q' == Char 'Q'
  void test_tdeck_controls()       // 0x08 Backspace; 0x0D Select; 0x0A Select
  void test_tdeck_unknown()        // 0x00, 0x1B, 0x7F, 0xB5 → None
  void test_backspace_consumed()   // resolveBackspace(true, 0) == Delete; (true, UINT32_MAX) == Delete
  void test_backspace_back()       // resolveBackspace(false, UINT32_MAX) == Back
  void test_backspace_guard_399()  // resolveBackspace(false, 399) == Drop
  void test_backspace_guard_400()  // resolveBackspace(false, 400) == Back
  void test_any_input_wakes()      // wakesScreen(Char 'a', true), (Select, true), (Up, true), (Backspace, true), (ScreenOff, true); !(None, true)
  ```
  In `test_keymap_web`, change the existing wake assertions to `wakesScreen(x, false)` and add `TEST_ASSERT_FALSE(wakesScreen(mapM9Key('a', false), false))`.
- [x] **Step 2: Run** `pio test -e native -f test_tdeck_input`. Expected: it fails to compile, because the headers are missing.
- [x] **Step 3: Implement** the three functions and update the `ui_task` call site.
- [x] **Step 4: Run** `pio test -e native` and `scripts/build-upload-monitor.sh -B`. Expected: green, and all four envs build.
- [x] **Step 5: Commit** `feat(input): T-Deck key map, Backspace-as-Back rule, any-input wake`.

### Task 3: Screens consume Backspace; the UI loop applies the rules

**Files:**
- Modify: `src/screens/router.h`, every screen whose `onKey` deletes text on `Action::Backspace`, and `src/app/ui_task.cpp`
  - Find them with `grep -n "Action::Backspace" src/screens lib/core/src/ui/widgets`. Today that is Room, CmailList (new-conversation input), Composer, Entry (reply) and LoginForm, plus any Palette or filter input.

**Interfaces:**
- Consumes: `resolveBackspace`, `kBackspaceGuardMs`, `wakesScreen(e, bool)` (Task 2); `backspaceIsBack`, `anyInputWakes` (Task 1); `Gestures::reset` (touch plan).
- Produces, on `Screen`:
  ```cpp
  // True while a focused text input has something to delete.
  virtual bool consumesBackspace() const { return false; }
  ```
- `ui_task::handleKey`, once the screen is on and before `handleGlobal`, when `board::backspaceIsBack()` and the action is `Backspace`:
  - `Delete`: `lastDeleteMs = now`, then pass the key on as Backspace.
  - `Back`: pass the key on as `Action::Back`.
  - `Drop`: return.
- **Touch wake** (`anyInputWakes()` only), in the per-frame touch block:
  - While the power is not `On` and `readTouch` reports a finger down, wake exactly as a waking key does.
  - Then set `touchSwallow = true` and `gestures.reset()`.
  - While `touchSwallow` is set, samples aren't fed to `gestures`. It clears on the first sample with no finger down.
- On the M9 and Wio both flags are false, so neither branch runs.

- [x] **Step 1: Implement** `consumesBackspace` on each text screen. It returns true while the focused input's text is non-empty. On a multi-field form, use the focused field.
- [x] **Step 2: Implement** the `handleKey` branch and the touch wake.
- [x] **Step 3: Run** `pio test -e native` and `scripts/build-upload-monitor.sh -B`. Expected: green, and all four envs build.
- [x] **Step 4: Check that M9 and Wio behaviour is unchanged.** Both new branches are gated on flags that are false on those boards. `consumesBackspace` only answers a question and doesn't change `onKey`.
- [x] **Step 5: Commit** `feat(app): Backspace acts as Back and any input wakes, on boards that ask`.

### Task 4: T-Deck envs, pins, power, panel, keyboard and touch

**Files:**
- Create: `src/hal/tdeck/pins_tdeck.h`, `src/hal/tdeck/board_tdeck.cpp`, `src/hal/tdeck/keyboard_tdeck.{h,cpp}`
- Modify: `platformio.ini`, `scripts/build-upload-monitor.sh`, `docs/BUILD.md`, `AGENTS.md`, `.github/workflows/build.yml`, and the `#if` guards that pick a HAL so that `BOARD_TDECK` compiles `src/hal/tdeck/` only
  - The M9 panel and Wio touch drivers must compile into the T-Deck build. Widen their guards to `BOARD_M9 || BOARD_TDECK` and `BOARD_WIO_L2 || BOARD_TDECK`.

**Interfaces:**
- Consumes: `st7789::Config`, `gt911::Config` (Task 1); `mapTdeckKey` (Task 2); `touchToFrame` (touch plan Task 1).
- Produces:
  - `pins_tdeck.h` (namespace `tdeck`), with every value from Global Constraints plus:
    ```cpp
    constexpr uint8_t PANEL_MADCTL = /* camillia's landscape value; confirm on hardware */;
    constexpr int PANEL_NATIVE_W = 240, PANEL_NATIVE_H = 320, PANEL_W = 320, PANEL_H = 240;
    // GT911 raw → landscape. touchToFrame(raw, NATIVE_W, NATIVE_H, TOUCH_ROT) gives the frame point.
    constexpr uint8_t TOUCH_ROT = /* from camillia's LGFX touch config; confirm on hardware */;
    ```
    Read camillia's T-Deck display and touch setup (`grep -rn "Touch_GT911\|DEVICE_TDECK" ../camillia/camillia-mt/src`) for the starting values. Put a `// confirm on hardware (TESTING.md)` comment on each.
  - `board_tdeck.cpp` implements every `board.h` function. Board-specific results:
    - `name()` returns `"TDeck"`.
    - `hasTouch()` is `gt911::present()`.
    - `hasKeyboard()`, `backspaceIsBack()` and `anyInputWakes()` are true.
    - `canRotate()` is false and `setRotation` does nothing.
    - `wakeHint()` returns `"press any key to wake"`.
    - `rtcRead` and `rtcWrite` return false.
    - The GPS, battery, beep and SD functions are stubs until Task 6: `hasGps()` false, `batteryPercent()` −1, `hasSdSlot()` false.
    - `pollKey` drains the keyboard only until Task 5 adds the trackball.
  - `begin()` runs in this order:
    1. `POWERON` HIGH;
    2. LoRa CS and SD CS outputs HIGH;
    3. `st7789::begin(config)`;
    4. `Wire.begin(18, 8)`;
    5. the keyboard probe: poll 0x55 for an ACK for up to 500 ms, because the C3 boots after `POWERON`;
    6. `gt911::begin({&Wire, 0x5D, nullptr, nullptr})`.
  - `backlight(on)`: LEDC on pin 42 at 12 kHz, 8-bit; on writes 255 and off writes 0.
  - `keyboard_tdeck`:
    ```cpp
    bool begin();               // true when 0x55 ACKs
    bool poll(uint8_t& raw);    // at most one Wire.requestFrom(0x55, 1) per 10 ms; false on 0 or no data
    ```
- `platformio.ini`:
  - `[env:tdeck]` copies `[env:m9]`'s board, memory, flash and partition lines, and adds `-DBOARD_TDECK -DBOARD_HAS_PSRAM -DARDUINO_USB_MODE=1 -DARDUINO_USB_CDC_ON_BOOT=1 -std=gnu++17`, plus the `upload_erase` script.
  - `[env:tdeck_fake]` extends it with `-DRC_FAKE_API=1`.
  - Add a comment block in the style of the Wio's.
- Build script (`scripts/build-upload-monitor.sh`), so the T-Deck works like the other boards:
  - `env_label`: `tdeck` → "LilyGO T-Deck Plus", `tdeck_fake` → "LilyGO T-Deck Plus (fake API)";
  - a `--tdeck|-t` board flag (`BOARD=tdeck`), so `-t`, `-t -f` (→ `tdeck_fake`), `-t -E` and `-t -B` all work;
  - the usage text: the flag in the `Usage:` line and the board-flags list, and `-t -f` → `tdeck_fake` in the `--fake` example;
  - the `-B` sweep and the interactive picker read `platformio.ini`, so they need no change; confirm both list the two new envs.
  - *Done ahead of the task (2026-10-27): the labels, the `-t` flag, the usage text and the BUILD.md/AGENTS.md mentions. Until the envs land, `-t` reports "Environment 'tdeck' not found". With the envs in, `-B` and the picker list all six (confirmed).*
- Docs for the script: add the `--tdeck`/`-t` row to the options table in `docs/BUILD.md` (and the `-t -f` example in its `--fake` row), and mention `-t` beside `-w` in `AGENTS.md`'s Commands block.
- CI: append `-e tdeck -e tdeck_fake`.

- [x] **Step 1: Implement** the above.
- [x] **Step 2: Run** `pio test -e native` and `scripts/build-upload-monitor.sh -B`. Expected: green, and **six** envs build.
- [x] **Step 3: Check** `pio run -e tdeck -t size` (or the build log) for no LovyanGFX in the link. Check that `grep -rn "BOARD_TDECK" src` shows only HAL selection guards and `src/hal/tdeck/`.
- [x] **Step 4: Commit** `feat(hal): T-Deck Plus board, panel, keyboard and touch`.

### Task 5: Trackball

**Files:**
- Create: `lib/core/src/input/click_timer.{h,cpp}`, `src/hal/tdeck/trackball_tdeck.{h,cpp}`
- Modify: `src/hal/tdeck/board_tdeck.cpp`, `test/test_tdeck_input/test_main.cpp`

**Interfaces:**
- Produces (namespace `rc`):
  ```cpp
  constexpr uint32_t kClickHoldMs = 2000, kClickMotionQuietMs = 160;
  class ClickTimer {
   public:
    void motion(uint32_t ms);              // a trackball move was emitted
    void press(uint32_t ms);
    void release(uint32_t ms);
    // Select: a tap, released before the hold time, once motion has been quiet
    // kClickMotionQuietMs (a deferred tap fires on the first poll after that).
    // ScreenOff: once, at kClickHoldMs of holding; that hold's release emits nothing.
    Action poll(uint32_t ms);              // None, Select or ScreenOff
  };
  ```
- Produces (`trackball_tdeck.h`, namespace `trackball`):
  ```cpp
  void begin();                    // INPUT_PULLUP on all five pins; FALLING ISRs count Up/Down/Left/Right pulses
  bool poll(rc::KeyEvent& key);    // one direction per 45 ms at most (then ClickTimer::motion), else the click
  ```
  - Each direction takes the largest pending count. Read the counts and zero them with interrupts masked, or use `std::atomic`.
  - The click is sampled by level each poll (LOW = pressed). Edges call `press` and `release`.
- `board::pollKey`: the keyboard first, then `trackball::poll`.

- [ ] **Step 1: Write the failing tests:**
  ```cpp
  void test_click_tap()                     // press 0, release 100, poll 100 → Select; poll 120 → None
  void test_click_hold_screen_off()         // press 0; poll 1999 None; poll 2000 ScreenOff; poll 2500 None
  void test_hold_no_select_on_release()     // …then release 3000; poll 3000 None; poll 4000 None
  void test_click_during_motion_deferred()  // motion 0, press 50, release 100: poll 100 None; poll 159 None; poll 160 Select
  void test_click_during_motion_extends()   // motion 0, press 50, release 100, motion 150: poll 200 None; poll 310 Select
  void test_motion_after_click_irrelevant() // press 0, release 50 → poll 50 Select; motion 60 changes nothing
  ```
- [ ] **Step 2: Run** `pio test -e native -f test_tdeck_input`. Expected: it fails to compile.
- [ ] **Step 3: Implement** `ClickTimer`, then the trackball HAL, then `pollKey`.
- [ ] **Step 4: Run** `pio test -e native` and `scripts/build-upload-monitor.sh -B`. Expected: green, and six envs build.
- [ ] **Step 5: Commit** `feat(hal): T-Deck trackball with click timing`.

### Task 6: GPS, battery, beep and SD

**Files:**
- Create: `lib/core/src/power/battery.{h,cpp}`, `src/hal/tdeck/gps_tdeck.cpp`, `src/hal/tdeck/audio_tdeck.{h,cpp}`, `src/hal/tdeck/sd_tdeck.cpp`
- Modify: `src/hal/tdeck/board_tdeck.cpp`, `src/hal/m9/board_m9.cpp`, `src/hal/wio_l2/board_wio_l2.cpp`, `test/test_tdeck_input/test_main.cpp`

**Interfaces:**
- Produces (namespace `rc`). This is today's duplicated M9 and Wio formula, moved:
  ```cpp
  // < 2.5 V → -1 (no reading); else (v - 3.3) / (4.15 - 3.3) * 100, rounded, clamped 0..100.
  int batteryPercentFromVolts(float v);
  ```
  The M9 and Wio call it in place of their inline copies.
- T-Deck:
  - `hasGps()` is true.
  - `gpsPower(true)` does `Serial1.begin(38400, SERIAL_8N1, 44, 43)`.
  - `gpsPower(false)` does `Serial1.end()`. The receiver stays powered, because the rail can't be switched alone (spec §4.1).
  - `gpsRead` reads whatever is available, up to `n`, and never blocks.
- Battery:
  - volts = the mean of 8 `analogReadMilliVolts(4)` readings × 2 × 1.03 / 1000.
  - `batteryPercent()` is `batteryPercentFromVolts(volts)`.
  - `externalPower()` is `HWCDC::isPlugged() || volts > 4.25f` (spec amendment).
- Beep, in `audio_tdeck`:
  ```cpp
  void begin();                    // ESP_I2S I2SClass, STD mode, 16 kHz, 16-bit mono, BCK 7 / WS 5 / DOUT 6
  void beep(int freqHz, int ms);   // returns at once; a task writes ms of square wave at amplitude 3000, then silence
  ```
  - A new beep while one is playing replaces it, through a one-slot queue.
  - Use a single long-lived task with `xTaskCreatePinnedToCore` and check its result.
- SD (`sd_tdeck.cpp`), per the backup plan's API:
  - `hasSdSlot()` is true.
  - `sdMount()` does `SD.begin(39, SPI, 20000000)`. If that fails, or `SD.cardType() == CARD_NONE`, it calls `SD.end()` and returns `nullptr`; otherwise it returns `&SD`.
  - `sdUnmount()` does `SD.end()`. LoRa CS 9 stays HIGH throughout.

- [ ] **Step 1: Write the failing test:**
  ```cpp
  void test_battery_percent()  // 2.49 → -1; 3.0 → 0; 3.3 → 0; 3.725 → 50; 4.15 → 100; 4.4 → 100
  ```
- [ ] **Step 2: Run** `pio test -e native -f test_tdeck_input`. Expected: it fails to compile.
- [ ] **Step 3: Implement** `batteryPercentFromVolts`, switch the M9 and Wio to it, then add the T-Deck GPS, battery, beep and SD.
- [ ] **Step 4: Run** `pio test -e native` and `scripts/build-upload-monitor.sh -B`. Expected: green, and six envs build.
- [ ] **Step 5: Commit** `feat(hal): T-Deck GPS, battery, speaker and SD`.

### Task 7: Docs

**Files:**
- Modify: `docs/USE.md`, `docs/TESTING.md`, `docs/BUILD.md`, `AGENTS.md`, `docs/superpowers/specs/2026-10-26-cyberspace-tdeck-plus-design.md`

- [ ] **Step 1: `USE.md`.** Add a T-Deck Plus controls section:
  - keys type;
  - Enter or a trackball click selects;
  - Backspace deletes while typing and goes back otherwise;
  - the trackball moves;
  - holding the click for 2 s turns the screen off;
  - any key, roll or touch wakes;
  - `⋯` or a long-press opens the Palette;
  - the setup AP name;
  - GPS power behaviour (the receiver stays powered).
- [ ] **Step 2: `TESTING.md`.** Add a "T-Deck Plus" section, unticked, with the spec §5 hardware list plus:
  - confirm `PANEL_MADCTL` and `TOUCH_ROT`;
  - SD backup and restore;
  - pull the card mid-backup: "SD write failed", and the panel keeps drawing.

  Stage only these hunks (`git apply --cached`), because the user's own ticks in that file stay unstaged.
- [ ] **Step 3: `BUILD.md` and `AGENTS.md`.**
  - Add the `tdeck` and `tdeck_fake` envs and the `-t` flag.
  - Add the T-Deck to the board list.
  - Update the native test count.
- [ ] **Step 4: Spec.** Add a "2026-10-26 amendments" note covering SD in scope and `externalPower`, and drop "the microSD card" from §1 Out of scope.
- [ ] **Step 5: Commit** `docs: T-Deck Plus controls, checks and build`.
