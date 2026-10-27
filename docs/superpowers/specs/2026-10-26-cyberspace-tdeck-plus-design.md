# LilyGO T-Deck Plus board — design

Status: approved in conversation 2026-10-26; awaiting written-spec review.

## 1. Goal

Run Rhino Cyberspace on the LilyGO T-Deck Plus. Its keyboard, trackball,
touch panel, GPS and battery should all work. Shared behaviour stays shared,
and the M9 and Wio L2 keep working exactly as they do today.

Out of scope: LoRa (SX1262), the keyboard backlight, the microphone, and the
original T-Deck without GPS. The original T-Deck is
likely to work, but it is not a target or tested.

Hardware reference: `../../camillia/camillia-mt/src/hal/hw_tdeck.h`, plus
`keyboard.cpp` (keyboard and trackball) and `gps.cpp` in the same project.

**2026-10-26 amendments (from the user):**
- **SD card in scope.** Backup runs on every hardware board, so the T-Deck's
  microSD is supported through the backup HAL API (`hasSdSlot`, `sdMount`,
  `sdUnmount`) on the shared SPI bus, CS 39 at 20 MHz, with LoRa CS 9 held
  high. It is mounted per operation, so the card stays removable.
- **External power.** `externalPower()` uses the Wio's rule,
  `HWCDC::isPlugged() || volts > 4.25`: the T-Deck's USB goes to the S3's own
  port, so a USB host is visible. This replaces "returns false" in §4.1.

## 2. Decisions (from the user)

| Topic | Decision |
|---|---|
| Home screen | The touch plan's big Home tiles. The trackball moves the selection, a click opens a tile, and a tap also works |
| Back | Backspace acts as Back whenever you are not typing (§4.2) |
| Screen off | Hold the trackball click for 2 s |
| Wake | Any key, trackball movement, or touch wakes the screen (this board only) |
| Order | After the GPS time, backup and touch plans; it reuses touch, the tiles and the GPS time service |

## 3. Hardware (from camillia-mt `hw_tdeck.h`)

| Part | Detail |
|---|---|
| SoC | ESP32-S3, 16 MB flash, 8 MB OPI PSRAM; console on native USB-CDC |
| Power | GPIO10 (`POWERON`) driven HIGH first thing at boot; it powers the peripherals, GPS included |
| Display | ST7789, 240×320 native, used as 320×240 landscape. SPI2 SCK 40 / MOSI 41 / MISO 38, CS 12, DC 11, no RST, backlight GPIO42 (PWM). Colours inverted |
| Shared SPI | LoRa CS 9 and SD CS 39 share the bus. Both are driven HIGH before the panel starts, so neither answers |
| Keyboard | ESP32-C3 keyboard controller, I2C 0x55 on SDA 18 / SCL 8 (`Wire`), INT 46. Reading one byte returns an ASCII key, or 0 for none |
| Touch | GT911 at 0x5D on the same bus, INT 16, no reset line |
| Trackball | Active-low GPIOs: up 3, down 2, left 1, right 15, click 0. Physical rolls map (per camillia, confirmed there): right → pin 2, left → pin 1, up → pin 15, down → pin 3 |
| GPS | L76K on UART1, RX 44 / TX 43, 38400 baud. Powered from the `POWERON` rail and cannot be switched on its own |
| RTC | None |
| Battery | ADC GPIO4, 1:2 divider, calibration ×1.03 (camillia `BATT_CAL`) |
| Audio | MAX98357A-class I2S amp: WS 5, BCK 7, DOUT 6. No buzzer |

Two values are confirmed at bring-up and recorded in `pins_tdeck.h`: the
panel rotation (MADCTL), and the touch transform from raw GT911 coordinates
to the landscape frame. Camillia's values are the starting point.

## 4. Architecture

### 4.1 HAL (`src/hal/tdeck/`)

These files implement `src/hal/board.h`:

- `pins_tdeck.h`: the pin table above.
- `board_tdeck.cpp`:
  - `begin()` order: `POWERON`, the SPI chip selects, the panel, the I2C bus, the keyboard probe, the trackball, the touch panel.
  - `name()` returns `"TDeck"`, so the setup AP is `Rhino-Cyberspace-TDeck-Setup`.
  - `hasTouch()` is true and `canRotate()` is false.
- **Panel:** the M9's `st7789` driver takes a config struct: SPI host and pins, CS, DC, RST (−1 for none), MADCTL and invert. The M9 passes its current values, so its behaviour doesn't change.
- **Touch:** the Wio's `gt911` driver takes the `TwoWire&` bus, the address and a transform. The Wio passes its current values.
- **`keyboard_tdeck.cpp`** polls 0x55 every 10 ms (no ISR) and maps each byte through `mapTdeckKey` (§4.2).
- **`trackball_tdeck.cpp`:**
  - FALLING-edge ISRs count pulses per direction.
  - `pollKey` drains them into Up/Down/Left/Right. One event is emitted per 45 ms at most, which is camillia's move debounce.
  - Click:
    - a click released before 2 s gives Select;
    - holding for 2 s gives ScreenOff, emitted once at the 2 s mark;
    - a click within 160 ms of motion is deferred until the motion stops (camillia's rule), so rolling doesn't click.
- **GPS:** `gpsPower(true)` starts `Serial1` at 38400 on 44/43. `gpsPower(false)` stops the UART. The receiver stays powered because the rail can't be switched alone, which costs about 20 mA while "off". The time service's on/off policy is unchanged.
- **RTC:** `rtcRead` and `rtcWrite` return false. The time service already handles boards without an RTC.
- **Battery:** the averaged ADC reading × 2 × 1.03 goes through the existing voltage-to-percent curve. `externalPower()` returns false, because the board has no charger sense.
- **`beep(freq, ms)`:** a background task writes a square wave of `ms` length to I2S (16-bit mono, 16 kHz) at a fixed low volume, then stops the channel. It returns immediately.

### 4.2 Input mapping (`lib/core`, tested on the host)

- **`mapTdeckKey(uint8_t)`** in `lib/core/src/input/keymap_tdeck.{h,cpp}`:
  - `0x08` → Backspace;
  - `0x0D`/`0x0A` → Select;
  - `0x20–0x7E` → Char;
  - everything else → None.
  - The C3 handles Alt, Sym and Shift itself and sends the resulting character.
- **Backspace as Back:**
  - The `Screen` base gains `virtual bool consumesBackspace() const { return false; }`. Screens with a focused text input override it to return true while that input is not empty.
  - When `board::backspaceIsBack()` is true (T-Deck only), the router turns a Backspace into Back if the current screen doesn't consume it.
  - **Repeat guard:** a Backspace that arrives within 400 ms of a Backspace that deleted text is dropped instead of becoming Back. This stops a held or fast Backspace from leaving the screen after it empties the input.
  - The rule is a pure function, `resolveBackspace(consumes, msSinceLastDelete)`, in `lib/core/src/input/`.
- **Wake:**
  - `wakesScreen(e)` becomes `wakesScreen(e, anyInputWakes)`. With `board::anyInputWakes()` true (T-Deck only), any key event wakes the screen, and so does a touch down.
  - The event that wakes the screen is consumed, so it doesn't also type or select.
  - On other boards the result is unchanged: only ScreenOff wakes, and a centre tap peeks.
- **Palette:** the T-Deck has no Ctrl key. The touch plan's status-bar `⋯` button and long-press open the Palette.

### 4.3 Touch-plan interaction

- Home tiles show when `board::hasTouch()` is true, so the T-Deck gets them. The trackball's Up/Down/Left/Right drive `tileMove`.
- The on-screen keyboard shows only when `board::hasTouch() && !board::hasKeyboard()`. That adds a new HAL function `hasKeyboard()`: true on the M9 and T-Deck, false on the Wio. A tap on a text field focuses it without opening the on-screen keyboard.
- `canRotate()` is false, so there is no Orientation row. The keyboard fixes landscape.

### 4.4 Build

- New PlatformIO envs `tdeck` and `tdeck_fake` in `platformio.ini`:
  - `-DBOARD_TDECK`;
  - `ARDUINO_USB_CDC_ON_BOOT=1`;
  - QIO OPI PSRAM, 16 MB, `default_16MB.csv`.
- They use the same platform and libraries as the existing envs. No LovyanGFX, LVGL or RadioLib.
- `scripts/build-upload-monitor.sh` accepts `tdeck`, and `-B` builds all six envs. The upload port is the USB-CDC `cu.usbmodem*`.

## 5. Testing

On the host (`pio test -e native`):

- `mapTdeckKey`: printable characters, Backspace, CR/LF, an unknown code, and 0.
- `resolveBackspace`: consumed, not consumed, and the guard at 399/400 ms.
- `wakesScreen` with and without `anyInputWakes`, including the existing M9 cases.
- Trackball click timing as a pure function: tap, a 2 s hold, and a click during motion.

Builds: all six envs build. The M9 and Wio behave as before, including the
shared panel and touch drivers with their configs.

Hardware (`docs/TESTING.md`, T-Deck Plus section, unticked):

- boot and splash;
- display orientation and colours;
- every key types the right character;
- Backspace deletes while typing and goes back otherwise;
- the trackball moves in all four directions and clicks;
- a 2 s hold turns the screen off;
- any input wakes the screen without typing;
- touch accuracy on the tiles and lists;
- a GPS fix and the time zone;
- battery percent is sensible;
- a beep plays;
- the setup AP name.

## 6. Build order

After the GPS time, backup and touch plans:

1. **Shared code:** the driver configs (st7789, gt911) and the new HAL
   functions (`hasKeyboard`, `backspaceIsBack`, `anyInputWakes`). The M9 and
   Wio are unchanged.
2. **lib/core with TDD:** `mapTdeckKey`, `resolveBackspace`, the
   `wakesScreen` change and the click timing.
3. **Router and screens:** `consumesBackspace`, and the wake path in `ui_task`.
4. **T-Deck HAL bring-up:** envs, pins, power, panel, keyboard, trackball and
   touch. Flash and confirm the rotation and touch transform.
5. **T-Deck GPS, battery and beep.**
6. **Docs:** `USE.md` (T-Deck controls), `TESTING.md` and `AGENTS.md`.
