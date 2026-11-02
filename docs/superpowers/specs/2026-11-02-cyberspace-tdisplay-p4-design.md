# LilyGO T-Display P4 (AMOLED) — design

Date: 2026-11-02. Status: built, not yet tried on hardware.
Plan: `docs/superpowers/plans/2026-11-02-cyberspace-tdisplay-p4.md`.

## 1. Goal

A firmware env `tdisplay_p4` for the LilyGO T-Display P4 V1.0 with the 4.1"
AMOLED, with every feature the 16 MB boards have, usable three ways:

- **Portrait**, touch only, with the docked on-screen keyboard.
- **Landscape**, touch only, with the docked on-screen keyboard.
- **Landscape with the keyboard expansion** clipped on: keys type and move
  the cursor, and the on-screen keyboard stays hidden.

The TFT-LCD version (HI8561, 540x1168) is out of scope. It will get its own
env later, so panel-specific code stays in its own files (§3.3).

## 2. Hardware

ESP32-P4 (V1.0 boards report an ECO2 boot ROM: the `esp32p4_es` SDK profile,
360 MHz), 16 MB flash, 32 MB PSRAM, and an ESP32-C6 for Wi-Fi over ESP-Hosted.
Hardware code is ported from wadamesh (`variants/tdisplay_p4/`, AMOLED tested
on hardware); pins agree with camillia-mt's `src/hal/hw_tdisplay_p4.h`, whose
Arduino build setup this env follows. LilyGO numbers the expander pins in
octal (IO00-IO07, IO10-IO17); the table gives bit numbers.

| Part | Pins / address | Notes |
| --- | --- | --- |
| XL9535 expander | I2C0 SDA 7 / SCL 8, 0x20, INT 5 | bits: 0 3V3 rail, 1 antenna select, 2 screen reset, 3 touch reset, 4 touch INT, 6 audio power, 9 GPS wake, 12 C6 enable, 13 SD power (active low), 14 radio reset, 15 radio DIO1 |
| RM69A10 AMOLED | MIPI-DSI, 2 lanes, 1000 Mbps | 568x1232 portrait, RGB565, DPI 60 MHz, HSYNC 50 HBP 150 HFP 50, VSYNC 40 VBP 120 VFP 80 |
| DSI PHY | LDO channel 3 at 1830 mV | 2500 mV leaves the panel dark (wadamesh) |
| GT9895 touch | I2C0, 0x5D | raw 1060x2400 |
| BQ27220 gauge | I2C0 | voltage and state of charge |
| PCF8563 RTC | I2C0 | |
| ES8311 codec | Wire1 on SDA 20 / SCL 21, 0x18 | I2S MCLK 13, BCLK 12, WS 9, DOUT 10; NS4150B amp, no enable pin |
| AW86224 haptic | Wire1 on 20/21, 0x58 | LRA, waveform played from its SRAM |
| Keyboard expansion | Wire1 on SDA 46 / SCL 45 | TCA8418 at 0x34 (7x10), XL9555 at 0x20 (bit 6 TCA reset, bits 3-5 LEDs); INT 48, backlight PWM 47 |
| L76K GPS | UART: P4 RX 22, TX 23, 9600 | wake on XL9535 bit 9 |
| microSD | SD_MMC CLK 43, CMD 44, D0-D3 39-42 | 4-bit |
| C6 (ESP-Hosted) | SDIO CLK 18, CMD 19, D0-D3 14-17 | enable on XL9535 bit 12 |
| SX1262 | SPI SCK 2, MOSI 3, MISO 4, CS 24, BUSY 6 | unused; reset, then put to sleep |
| Side button | GPIO35, active low | |

## 3. Design

### 3.1 Build

- Env `tdisplay_p4`, the project's pioarduino platform (55.03.312, Arduino
  3.3.12). Its libs already carry `esp32p4_es` and the ESP-Hosted 2.12.13 C6
  images, so no separate toolchain or package directory is needed.
- `boards/tdisplay_p4.json` from camillia-mt (`esp32p4_es`, 360 MHz, PSRAM
  200 MHz, USB-Serial-JTAG console: `ARDUINO_USB_MODE=1`,
  `ARDUINO_USB_CDC_ON_BOOT=1`). PSRAM at 200 MHz needs
  `CONFIG_IDF_EXPERIMENTAL_FEATURES` in the prebuilt libs; if the libs run it
  slower and the DPI underruns (`can't fetch data from external memory fast
  enough`), lower the DPI clock rather than patch the SDK.
- `partitions_16mb.csv`, release with OTA, slug `tdisplay-p4`.
- Wi-Fi is Arduino `WiFi`, which the P4 core routes through ESP-Hosted to the
  C6. The C6 needs the matching ESP-Hosted 2.12.13 image (the one camillia-mt
  ships); docs/BUILD.md says how to flash it. P4 OTA does not update the C6.
- Board name `TDisplayP4`. The setup AP is `Monster-Cyberspace-TDisplayP4`,
  since `-Setup` would make it 35 characters.

### 3.2 Orientation and the keyboard

- Rotation r is ours (`ui/rotation.h`): 0 is the board's landscape, chosen as
  the landscape with the keyboard expansion below the screen. Which way the
  panel turns for that is one constant, `kKbLandscape`, confirmed on hardware.
- **The keyboard is detected once, at boot**, in `board::begin()` (the
  expansion is only fitted with the power off). Detection: the expansion's
  XL9555 answers at 0x20 on the 46/45 route, its TCA8418 is reset through
  it, and the TCA8418 answers at 0x34.
- **Keyboard found:** the board turns to rotation 0 and stays there.
  `canRotate()` is false, so Settings and web config hide Rotation and
  `ui_task` never calls `setRotation`; the saved rotation is left untouched.
  `hasKeyboard()`, `hasKeys()` and `hasKbLight()` are true: no on-screen
  keyboard, a cursor is drawn, and Backspace means Back (`backspaceIsBack`).
- **No keyboard:** `canRotate()` is true and the saved rotation applies. Touch
  only, no cursor, the docked on-screen keyboard.
- Taking the keyboard off and booting again brings back the saved rotation.
- **First boot:** a new `board::defaultRotation()` (default 0) is used when no
  rotation is saved. The T-Display P4 returns 1, Portrait.
- **Keys** (`lib/core/src/input/keymap_tdisplay_p4`, from wadamesh's
  `P4Keyboard.cpp` map): letters, digits and symbols with Shift, Sym and Caps
  Lock, as one-shot modifiers that lapse after 1.5 s. Arrows are Up, Down,
  Left and Right; Enter is Select; Esc is Back; Tab is Tab; Backspace
  repeats; the emoji key opens the emoji tray. F11 steps the backlight
  (`KbLight`, saved as the other boards save it). The expansion's LEDs light
  with Caps Lock.

### 3.3 Display

- `src/hal/tdisplay_p4/` holds wadamesh's `esp_lcd_rm69a10` vendor driver
  and a `panel_rm69a10` that brings the panel up in wadamesh's order: the
  rails power-cycled through the XL9535 (VCCA low; 5V high, low, high; 3V3
  low, high, low; 200 ms each), the DSI-PHY LDO, screen reset high, low, high
  (200 ms each), the DSI bus, DBI IO, DPI panel (one frame buffer in PSRAM,
  DMA2D), init, display on, then brightness with DCS 0x51.
- **2x scale.** The UI sees a 284x616 panel (616x284 in landscape):
  `panelWidth`/`panelHeight` follow the rotation. `pushRows` writes each
  logical pixel as a 2x2 block, rotated, straight into the DPI frame buffer,
  then writes the cache back for the rows it touched. The index arithmetic is
  a portable `lib/core/src/ui/scale2x` with native tests.
- A rotation change clears the frame buffer.
- `backlight`/`backlightLevel` set DCS 0x51; `displaySleep` sends display
  off and sleep in, and the reverse.
- **Restarts.** `esp_restart()` on the P4 leaves the DSI host half set up and
  the panel dark. As in wadamesh, a shutdown handler arms the RTC watchdog with
  a system reset, so every restart is a full reset.

### 3.4 Touch

GT9895 at 0x5D, read on I2C0, reset through the XL9535. A raw point
(0..1059, 0..2399) scales to the 568x1232 panel, halves to 284x616, then
turns to the current rotation (`ui/scale2x`, with tests). `hasTouch()` is
true with or without the keyboard.

### 3.5 Shared Wire1

The P4 has two general-purpose I2C controllers and I2C0 is the main bus, so
the keyboard (46/45) and the codec and haptic driver (20/21) take turns on
Wire1. `p4_i2c1` (from wadamesh's `P4I2c1`) holds a mutex, re-pins Wire1 to
the route asked for, and hands it back. Every Wire1 transaction sits between
acquire and release.

### 3.6 Sound and haptics

- `board::beep` queues a tone for a task that plays it through the shared
  `es8311` driver (the Wio's) on I2S, 44.1 kHz, 16-bit, the codec brought up
  on the first beep.
- New HAL calls, defaulted in `board_defaults.cpp`: `hasHaptic()` (false) and
  `haptic(int ms)` (a no-op). On this board, `haptic` queues a buzz for its
  own task (separate from the beep task), which plays wadamesh's AW86224 RAM
  waveform, looped for `ms`.
- New settings in `rc::Settings`: `tapVibrate` and `alertVibrate`, both true by
  default. They're shown only where `board::hasHaptic()`, in device Settings
  ("Tap vibration: On/Off", "Alert vibration: On/Off"), in web config (two
  checkboxes, applied through the UI task like `buzzer`) and in backup YAML
  (`tapVibrate`, `alertVibrate`). NVS keys `tapVib`, `alertVib`.
- **Tap buzz:** about 15 ms when a touch resolves to a widget (a tap that
  hits a target), if `tapVibrate`.
- **Alert buzz:** about 120 ms everywhere the UI beeps for an alert today
  (`ui_task.cpp`), if `alertVibrate`, independent of `buzzer`.

### 3.7 Other parts

- **Side button** (GPIO35): a press toggles the screen (off when on, on when
  off), like the other boards' wake key. `wakeHint()` is "press the side
  button to wake". `anyInputWakes()` is true with the keyboard fitted.
- **GPS:** `gpsPower(on)` drives XL9535 bit 9 high and opens the UART at
  9600; off reverses it.
- **RTC:** PCF8563, read and written in UTC; the voltage-low flag makes
  `rtcRead` false.
- **Battery:** BQ27220 state of charge and voltage; `externalPower()` from its
  current sign.
- **SD:** SD_MMC 4-bit, powered through XL9535 bit 13 (active low) for each
  mount, as the other boards mount per operation.
- **LoRa:** the SX1262 is reset through the XL9535, then sent `SetSleep`.
- **Notification light:** none.

## 4. App changes made with this board (all boards)

- `board::defaultRotation()`, read by `settings::load` when NVS has no `rot`.
- `board::hasHaptic()` / `board::haptic(ms)`, the two vibration settings, and
  their Settings items, web config fields and backup keys.
- No other screen changes: the keyboard, rotation and touch paths already
  exist for other boards.

## 5. Testing

- Native, written first: `test_tdisplay_p4_input` (key numbers and modifiers
  to portable keys, Caps Lock, one-shot lapse, repeat), `test_scale2x`
  (logical to frame buffer for all four rotations, raw touch to logical,
  corners and edges), and backup YAML round-trips for `tapVibrate` and
  `alertVibrate`, including a file without them.
- `pio test -e native`, `pio run -e tdisplay_p4`, and
  `scripts/build-upload-monitor.sh -B` for every env.
- Nothing is flashed by an agent. docs/TESTING.md gets the open hardware
  checks: the panel lights and the colours are right; touch lands where
  tapped in all four rotations; the keyboard is found at boot and the screen
  sits above it (`kKbLandscape`); keys, modifiers, repeat, F11 and the LEDs;
  Wi-Fi through the C6; beeps; both vibrations and their settings; GPS time;
  RTC; battery; SD backup; the side button; a restart from Settings leaves
  the panel lit.

## 6. Docs and tooling

`scripts/build-upload-monitor.sh` (`-4`), `scripts/release.sh` (env and
slug), `.github/workflows/build.yml`, the flasher (`site/index.html`),
AGENTS.md (board list, layout, commands, progress), docs/BUILD.md (the env
and the C6 image), docs/USE.md (the three ways to hold it, the vibration
settings) and docs/TESTING.md.
