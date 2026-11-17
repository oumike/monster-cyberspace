# Monster for Cyberspace on the M5Stack Tab5 — design

Date: 2026-11-17. Status: approved to build (user: "lets get to porting"; UI
at 2x, landscape with the keyboard and any of four rotations without it,
released and in the flasher right away).

## 1. Goal

A new build target, env `tab5`, for the **M5Stack Tab5** (SKU C145/K145): an
ESP32-P4 tablet with a 5" 720x1280 MIPI-DSI touch panel, an ESP32-C6 for
Wi-Fi, and the optional **Tab5 Keyboard** (SKU A164), a 70-key keyboard on
Ext.Port1 run by an STM32F030 over I2C.

References: M5Stack's docs (docs.m5stack.com/en/core/Tab5,
/en/tab5/Tab5_Keyboard, the keyboard's I2C protocol PDF and user manual) and
code: `m5stack/M5Tab5-UserDemo` (`platforms/tab5/`), `m5stack/M5Tab5-Keyboard-UserDemo`
(its `m5_tab5_component`: panel drivers, variant autodetect, pin map),
`m5stack/M5Tab5-Keyboard-Internal-FW` (the keyboard's register map and key
table), `m5stack/M5GFX` (`board_detect/m5/esp32p4.inl`, `Touch_ST7123`,
`extras/board_spec/boards/m5stack_tab5.json`) and `m5stack/M5Unified`
(`Power_Class.inl`: expanders, INA226, power off; ES8388 setup). The survey
with file and line references is the session note behind this spec; the
values it relies on are below.

The user's unit: ESP32-P4 revision v1.3 (esptool), the ST7123 panel.

## 2. Hardware summary

| Part | Detail |
|---|---|
| SoC | ESP32-P4 v1.x (`esp32p4_es` SDK profile, as the Tanmatsu), 16 MB flash, 32 MB PSRAM, bootloader at 0x2000 |
| System I2C | Wire, SDA 31, SCL 32, 400 kHz: expanders 0x43/0x44, INA226 0x41, RTC 0x32, ES8388 0x10, ES7210 0x40 (unused), touch 0x55 (ST7123/ST7121) or 0x14/0x5D (GT911), BMI270 (unused) |
| Expander 0x43 (PI4IOE5V6408) | P0 antenna select (low internal), P1 speaker amp, P2 5 V out (Grove, M-Bus, Ext.Port), P4 LCD reset, P5 touch reset, P6 camera reset, P7 headphone detect (in) |
| Expander 0x44 | P0 C6 power, P3 USB-A 5 V, P4 power-off pulse, P5 charge-current step (low 1 A), P6 charge status (in), P7 charger enable |
| Display | 720x1280 portrait native, RGB565 over MIPI-DSI, 2 lanes. DSI PHY LDO channel 3 at 2500 mV. Backlight GPIO22 (LEDC PWM). Three panels: ST7123 (integrated touch), ST7121 (integrated touch), ILI9881C (+ GT911) |
| Touch | ST7123/ST7121: I2C 0x55. GT911: 0x14 or 0x5D. INT GPIO23; reset expander 0x43 P5 |
| Wi-Fi | ESP32-C6 over SDIO, 4-bit: CLK 12, CMD 13, D0 11, D1 10, D2 9, D3 8, slave reset GPIO15; powered by expander 0x44 P0. Factory C6 image: ESP-Hosted 1.4.1 |
| Audio | ES8388 at 0x10; I2S MCLK 30, BCLK 27, WS 29, DOUT 26; NS4150B amplifier on expander 0x43 P1 |
| Battery | 2S NP-F550 (Tab5 Kit only). INA226 0x41, shunt 5 mΩ: bus voltage = pack voltage; current sign gives charging |
| RTC | RX8130CE at 0x32, BCD time at 0x10–0x16 |
| SD | SDMMC slot 0, 4-bit: CLK 43, CMD 44, D0–D3 39–42; IO power from on-chip LDO channel 4 |
| Keyboard | Tab5 Keyboard on Ext.Port1: I2C 0x6D on SDA GPIO0 / SCL GPIO1 (Wire1), INT GPIO50 (active low); two WS2812 LEDs |
| Buttons | Power (hardware) and Reset/Boot: neither readable by software |

## 3. Display

- `panel_tab5.cpp`, modelled on the T-Display P4's `panel_rm69a10.cpp` and
  the Tanmatsu's `panel_st7701.cpp`: LDO 3 at 2500 mV, release the resets on
  0x43, DSI bus (2 lanes, 1040 Mbps), DBI command IO, then each panel's init
  list (from `M5Tab5-Keyboard-UserDemo`'s `esp_lcd_st7123.c`,
  `esp_lcd_st7121.c`, `esp_lcd_ili9881c.c`) and a DPI panel with one PSRAM
  frame buffer flushed out of the cache by rows (`esp_cache_msync`).
- **Which panel** (M5's autodetect): release LCD_RST and TP_RST (0x43
  direction 0x30 … the reset bits as inputs with pull-up; LCD_RST must never
  be driven high), drive touch INT high and probe 0x14 and 0x5D: an answer
  means ILI9881C + GT911. Otherwise read the ST touch's firmware version
  (0x55, 16-bit register 0x0000): 3 is ST7123, 1 ST7121, anything else
  ST7123.
- Timings (hsync/hbp/hfp, vsync/vbp/vfp; DPI clock): ST7123 2/40/40,
  2/8/220, 80 MHz; ST7121 2/40/40, 20/24/220, 80 MHz (M5GFX runs it at
  70 MHz and 900 Mbps; tuned if it misbehaves); ILI9881C 40/144/40, 4/20/108,
  80 MHz.
- **2x, four rotations:** the UI is drawn at 2x through `ui/scale2x`, a
  360x640 portrait or 640x360 landscape logical panel (80x22 cells of 8x16
  text). Without the keyboard any of four rotations (`canRotate()`), default
  landscape; with the keyboard fitted it locks to the landscape rotation that
  sits upright above it (`kKeyboardRotation`, tuned on hardware).
  `defaultRotation()` is that landscape rotation too.
- Backlight: GPIO22, LEDC 12-bit at 5 kHz; `backlightLevel` maps 0–255.
- A software restart becomes a full system reset through the RTC watchdog,
  as on the other P4 boards (a CPU reset leaves the DSI host half set up).

## 4. Touch

`touch_tab5.cpp`: the ST7123/ST7121 report as M5GFX's `Touch_ST7123` reads
them; the GT911 through the shared `gt911` driver. Raw coordinates are the
native 720x1280 portrait frame, mapped through `scale2x` for the rotation as
on the T-Display P4. One touch point is enough.

## 5. Keyboard

Detected once in `board::begin()` (probe 0x6D on Wire1, read the version at
0xFE); fitted with the power off, like the T-Display P4's expansion. It runs
in **Normal mode** (register 0x10 = 0): every press and release arrives as one
byte, bit 7 pressed, bits 6–4 row, bits 3–0 column, popped from register 0x20.
Read while INT (GPIO50) is low, then write 0 to 0x01 to release INT. The
keyboard firmware's own Aa and Sym handling only runs in its HID and Char
modes, so `lib/core/src/input/keymap_tab5` (`Tab5Keys`, native tests in
`test/test_tab5_input`) applies the layers:

Matrix (row, columns 0–13; "base/Sym" where the key has a second layer),
from the firmware's `user_keyboard_handle.c` (`key_value_map`,
`key_modifier_flag`):

| Row | Keys |
|---|---|
| 0 | esc 1 2 3 4 5 6 7 8 9 0 - + del |
| 1 | \`/~ !/? @ # $ % ^ & \*// (/< )/> [/{ ]/} \\/\| |
| 2 | tab q w e r t y u i o p ;/: '/" backspace |
| 3 | Sym Aa a s d f g h j k l up \_/= enter |
| 4 | Ctrl Alt z x c v b n m ./, left down right space |

| Key | Action |
|---|---|
| Letters | Char; Aa upper-cases them |
| Aa | Tapped: the next letter upper case. Double tap (500 ms): Caps Lock. Held: upper case while held. As the keyboard's own firmware |
| Sym | Held: the second layer of the keys that have one. Tapped alone: the next key only |
| Digits, punctuation | Char; Sym, or Aa (as Shift), gives the second layer of the keys above that have one |
| Esc | Back |
| Enter | Select |
| Tab | Palette (`menuKeyName` "tab") |
| Backspace, del | Backspace (Back on an empty line: `backspaceIsBack`), repeating when held |
| Arrows | Up/Down/Left/Right, repeating when held |
| Space | Char ' ' |
| Ctrl tapped alone | `Action::Emoji` (the actions, then the emoji tray, while typing; the palette elsewhere), as the M9 and the CardputerZero |
| Alt + 1…7 | F1…F7: Home, Messages, C-Mail, Feed, Guilds, Profile, Settings |
| Alt + 0 | F10, the key help |
| Ctrl or Alt held + another key | Types nothing (Alt + a digit as above) |

`board::functionKeys()` gives the F10 help the Tab5's table (`Alt+1` …,
`Ctrl`, `Tab`). With the keyboard: `hasKeyboard`, `hasKeys`,
`backspaceIsBack` and `anyInputWakes` are true. Without it the Tab5 is a touch
board like the CrowPanels (docked on-screen keyboard, double tap wakes).

**Keyboard LEDs** (custom mode, register 0x11 = 1, brightness 0x03): LED 0
shows Caps Lock (white) or a pending Aa (dim white); LED 1 is the
notification light (`hasNotifyLight` while the keyboard is fitted; rooms and
mentions, as the Mesh Deck's front LED).

## 6. Wi-Fi through the C6

`board::begin()` power-cycles the C6 through expander 0x44 P0 (off 100 ms, on
100 ms) and calls `hostedSetPins(12, 13, 11, 10, 9, 8, 15)` before WiFi
starts, as M5Unified does. Antenna: internal (0x43 P0 low). The C6 keeps
M5's factory ESP-Hosted image (1.4.1); UIFlow's 2.12 host talks to it, so
Arduino's 2.12.13 host is expected to. That is the first hardware check; this
firmware never flashes the C6. M5's restore guide
(docs.m5stack.com … tab5/c6_wifi_restore) covers a C6 that lost its image.

## 7. Other peripherals

- **Battery:** INA226 configured as M5Unified (16 averages, 1.1 ms
  conversions, continuous), bus voltage register 0x02 × 1.25 mV = pack
  voltage. Per cell = half; percentage `(cell − 3300) × 100 / 800` clamped,
  as M5Unified. Below 5 V (no pack: the plain C145 runs from USB) gives no
  reading. Charging when the shunt voltage shows current into the pack
  (sign as M5Unified, tuned on hardware). Read every 5 s. The charger is
  enabled at boot (0x44 P7 high, P5 low for 1 A) as M5Unified does.
- **RTC:** RX8130CE, BCD at 0x10–0x16 (24 h); set by NTP like the other RTCs.
- **SD:** mounted through ESP-IDF's `esp_vfs_fat_sdmmc_mount` with on-chip
  LDO 4, as the T-Display P4 (the generic esp32p4 variant's `SD_MMC` drives
  GPIO45 as a power pin).
- **Sound:** ES8388 + NS4150B: M5Unified's register sequence, Arduino
  `ESP_I2S` at 48 kHz with MCLK, the amplifier (0x43 P1) on only around a
  beep, volume through the DAC volume registers. `hasVolume` true.
- **5 V out** (0x43 P2) on, as M5's code leaves it (the keyboard may need
  it). USB-A 5 V (0x44 P3) on.
- **None:** GPS, haptics, keyboard backlight, readable buttons.
- `board::wakeHint()`: "double-tap or press a key to wake".

## 8. Build, release, docs

- `boards/tab5.json` (from `tanmatsu.json`, `esp32p4_es`), env `tab5`
  (`BOARD_TAB5`, `partitions_16mb.csv`), `board::name()` "Tab5",
  `board::otaSlug()` "tab5".
- CI builds it; `scripts/build-upload-monitor.sh` takes `--tab5`/`-5`… (a free
  letter); `-B` includes it.
- **Released and in the flasher from the start** (user decision):
  `scripts/release.sh`'s env list (bootloader at 0x2000 as the other P4s),
  the flasher's board list and `scripts/pages-site.sh`.
- AGENTS.md, README.md, docs/BUILD.md, docs/USE.md (keys), docs/TESTING.md
  (hardware check).

## 9. Hardware checks (docs/TESTING.md)

Boots and finds the panel (which one, logged); picture upright and colours
right in all four rotations; touch lands under the finger in each; backlight
levels; keyboard found, every key and layer, Aa tap/double/hold, Sym, Ctrl,
Alt + digits, repeats, rotation locked; keyboard LEDs; Wi-Fi through the C6
(the logged host and slave versions); battery percentage and charging (Kit);
RTC keeps time over a power cycle; SD backup and restore; beep and volume;
restart comes back with the panel lit; OTA update.
