# Attaky Mesh Deck 1.0 — port plan

Survey approved 2026-10-29 (autonomous mode); see the summary in
`docs/superpowers/specs/2026-10-29-cyberspace-mesh-deck-design.md`.

## 1. Identity

- firmware-name: Monster for Cyberspace
- version: the release version (`RC_VERSION`), currently 0.1.3
- build-SKU: Mesh Deck 1.0 (`Core_ESP32_1.0` + `Expand_LoRa-GPS_1.0` +
  `Control_Keyboard_(Bottom)_1.0` + `Power_Standard-Cell_1.0`)

## 2. Changes

New files (the board seam, `src/hal/mesh_deck/`):

- `pins_mesh_deck.h`: pins and addresses (catalog; camillia-mt `hw_mesh_deck.h`,
  platy-aprs `pins_mesh_deck.h`).
- `io_mesh_deck.{h,cpp}`: I2C bus recovery, the AW9523s (front 0x59, keyboard
  0x5A/0x5B), FT6636 touch (polled), MAX17048 gauge, the RGB LEDs.
- `board_mesh_deck.cpp`: `board::` for the ST7789 (shared `st7789` driver,
  landscape MADCTL 0xA0, panel mounted 180°), buttons, keyboard, touch,
  battery, notification light.
- `gps_mesh_deck.cpp`: ATGM336H on Serial1 (9600 baud), no RTC, no SD.
- `lib/core/src/input/keymap_mesh_deck.{h,cpp}`: matrix positions and the
  D-pad to portable keys, with native tests.

Notification light (all boards, shown only where `board::hasNotifyLight()`):

- `board::hasNotifyLight()` / `board::notifyLight(rgb)`; false / no-op elsewhere.
- `lib/core/src/ui/notify_light.{h,cpp}`: colours, timeouts, and the blink
  pattern (platy-aprs `notify_led.cpp`: a double blink each second while
  mentions or C-Mail are unread, up to the timeout), with native tests.
- Settings `notifyLight` (colour, default Blue) and `notifyTimeout` (seconds,
  default 15, 0 = until read): NVS, backup YAML, web config, device Settings.

Edits: `platformio.ini` (`[env:mesh_deck]`), `src/hal/board.h`, the other
HALs (stubs), `scripts/build-upload-monitor.sh`, `scripts/release.sh`, CI,
docs (AGENTS.md, USE.md, TESTING.md, BUILD.md).

## 3. Boot banner

`board::begin()` prints
`[INFO] [APP] Monster for Cyberspace v<RC_VERSION> on Mesh Deck 1.0` and
`[INFO] [APP] Build: <date time> / toolchain: PlatformIO`.

## 4. Milestones (each needs device evidence)

- M0: `pio run -e mesh_deck` builds; the other envs and `pio test -e native` stay green.
- M1 display: the banner in the boot log, the splash upright and not mirrored.
- M2 input: D-pad directions match the arrows, L1 Back, R2 screen off and on,
  keyboard typing, touch lands under the finger.
- M3 power: battery % in the status bar, charging shown on USB.
- M4 GPS: NMEA read, the time set from GPS.
- M5 light: the LEDs blink in the chosen colour on a mention.

## 5. Toolchain

The repo's `[env]` platform (pioarduino, Arduino 3.x), esp32-s3-devkitc-1,
qio_opi, 16 MB, `partitions_16mb.csv`. Console on UART0 through the CH340
(`ARDUINO_USB_CDC_ON_BOOT=0`, `ARDUINO_USB_MODE=0`).
