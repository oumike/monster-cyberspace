# Monster for Cyberspace on the Tanmatsu — design

Date: 2026-11-04. Status: approved to build (user: "Once you've got a plan, go
straight to development").

## 1. Goal

A new build target, env `tanmatsu`, for the Nicolai Electronics / badge.team
**Tanmatsu**: an ESP32-P4 handheld with a 3.97" 480x800 MIPI-DSI panel, a full
keyboard, an ESP32-C6 for Wi-Fi and a CH32V203 coprocessor that owns the
keyboard, backlights, LEDs, power and RTC. The firmware is **standalone**
(user decision): a PlatformIO env like `tdisplay_p4` whose image replaces the
Tanmatsu launcher, not an AppFS app.

References: wadamesh `tanmatsu/` (its `main/main.cpp`, `TANMATSU_PORT.md`,
`sdkconfig`) and its managed components `badgeteam__badge-bsp`
(`targets/tanmatsu/`), `nicolaielectronics__tanmatsu_coprocessor`,
`nicolaielectronics__mipi_dsi_abstraction` (the ST7701 init list),
`nicolaielectronics__es8156`, and docs.tanmatsu.cloud (hardware, LEDs,
buttons).

## 2. Hardware summary

| Part | Detail |
|---|---|
| SoC | ESP32-P4 engineering silicon (`esp32p4_es`), 16 MB flash, 32 MB PSRAM, bootloader at 0x2000 |
| Internal I2C | Wire, SDA 9, SCL 10, 400 kHz: coprocessor 0x5F, ES8156 0x08, BMI270 0x68 (unused) |
| Coprocessor | CH32V203, plain register file: write `[reg, data…]`, read by writing `reg` then reading. Interrupt on GPIO1 (unused: polled) |
| Display | ST7701 over MIPI-DSI, 2 lanes at 500 Mbps, DPI 30 MHz, native 480x800 portrait, RGB565. DSI PHY LDO channel 3 at 2.5 V, reset GPIO14. Backlight: coprocessor register 11 |
| Keyboard | 9-byte bitfield at coprocessor register 2 (layout in §4). Its light: register 12 |
| Buttons | Volume down GPIO35 (low when pressed). Power button: coprocessor input register 15 bit 2 (held 2 s, the hardware cuts power) |
| LEDs | Six RGB LEDs, GRB, registers 127–144; mode register 145 (bit 0 automatic) |
| Wi-Fi | ESP32-C6 over SDIO slot 1: CLK 17, CMD 16, D0–D3 18–21. Powered and reset by coprocessor register 17 (0 off, 1 bootloader, 2 application) |
| Audio | ES8156 DAC, I2S MCLK 30, BCLK 29, WS 31, DOUT 28; amplifier enable is coprocessor output register 16 bit 0 |
| Battery | PMIC through the coprocessor: VBAT mV at 109–110 (ADC triggered by register 108), charging status register 120 |
| RTC | Coprocessor registers 18–21: uint32 seconds, LSB first |
| SD | SDMMC slot 0, 4-bit: CLK 43, CMD 44, D0–D3 39–42 |
| LoRa | Behind the C6 (on the Tanmatsu's LoRa add-on); untouched |

## 3. Display

- `panel_st7701.cpp`, modelled on the T-Display P4's `panel_rm69a10.cpp`, sends
  the Tanmatsu ST7701 init list from `dsi_panel_nicolaielectronics_st7701.c`
  over the DBI IO, then runs the DPI panel from one PSRAM frame buffer, flushed
  out of the cache by rows (`esp_cache_msync`).
- The Tanmatsu is landscape with its keyboard below: the panel's 480x800
  portrait frame is turned 270° (`PANEL_ROTATION`, the BSP's default; tuned on
  hardware) and the UI is drawn **at 2x** through `ui/scale2x`, a 400x240
  logical panel. At about 235 ppi, 1x text would be too small to read.
  `canRotate()` is false.
- Brightness goes to coprocessor register 11, not DCS.
- A software restart becomes a full system reset through the RTC watchdog, as
  on the T-Display P4 (a P4 CPU reset leaves the DSI host half set up).

## 4. Input

The keyboard state is polled from coprocessor register 2 every UI loop (at most
every 15 ms) and the 9 bytes are diffed in `lib/core/src/input/keymap_tanmatsu`
(`TanmatsuKeys`, native tests in `test/test_tanmatsu_input`). Unlike the
T-Display P4's TCA8418, this is a real matrix with held state, so modifiers are
**held**, not one-shot.

Bit order, byte by byte (bit 0 first):
`esc f1 f2 f3 ` 1 2 3 | tab q w e fn a s d | shiftL z x c ctrl meta altL \ |
4 5 6 7 r t y u | f g h j v b n m | f4 f5 f6 backspace 9 0 - = |
o p [ ] l ; ' return | . / up shiftR altR left down right |
8 i k , spaceL spaceM spaceR volumeUp`.

| Key | Action |
|---|---|
| Letters, digits, punctuation | Char; Shift (either) gives the shifted US character |
| AltGr (right Alt) | The BSP's AltGr layer (precomposed Latin-1 letters and signs only; its combining accents are left out), Shift+AltGr its shifted layer |
| Space (all three) | Char ' ' |
| Esc | Back |
| Return | Select |
| Arrows | Up/Down/Left/Right, repeating when held |
| Backspace | Backspace, repeating when held |
| Tab | Palette |
| F1–F6 | `functionKeyAction(1..6)`: Home, Messages, C-Mail, Feed, Guilds, Profile |
| Fn+F1 | Settings (the P4's F7) |
| Fn+F4 | Function-key help (the P4's F10) |
| Fn+F5 | Keyboard-light step (the P4's F11) |
| Fn+Space | Emoji |
| Meta (the logo key), tapped alone | Home |
| Ctrl, left Alt, Meta held | The key they modify types nothing |
| Volume up (keyboard) / volume down (GPIO35) | Up / Down |
| Power button | ScreenOff (lock, or wake) |

`board::functionKeys()` (new, weak default: `kFunctionKeys`) lets the F10 help
show the Tanmatsu's own table (`kTanmatsuFunctionKeys`: F1–F6, Fn+F1, Fn+F4,
Fn+F5). The key column widens from 4 to 5 cells for "Fn+F1".

`hasKeyboard`, `hasKeys`, `backspaceIsBack` and `anyInputWakes` are true; no
touch.

## 5. Wi-Fi through the C6

Arduino's ESP-Hosted host (`esp32p4_es` libs: C6 target, SDIO slot 1, 4-bit,
40 MHz) matches the Tanmatsu's own sdkconfig. `board::begin()`:

1. turns the C6 off and on in application mode through coprocessor register
   17 (100 ms after each write), so it boots fresh with the host;
2. calls `hostedSetPins(17, 16, 18, 19, 20, 21, 54)` before WiFi starts.
   GPIO54 is a free expansion pin standing in for the reset line the
   coprocessor really owns.

The C6 runs Nicolai's `esp-hosted-tanmatsu` (2.12.x). Whether Arduino's host
talks to it unchanged is the first hardware check; the C6 is never flashed by
this firmware.

## 6. Other peripherals

- **Keyboard light:** register 12, `KbLight::Medium` by default, off with the
  screen.
- **Notification light:** LED 2, the "messages" LED. `board::begin()` sets the
  LEDs to manual (register 145 = 0) and lights LED 0 (power) dim green itself,
  since manual mode stops the coprocessor's own power LED.
- **Battery:** VBAT (continuous ADC enabled at boot) mapped to a percentage
  with the BSP's curve, `123 − 123 / (1 + (v/3.7)^80)^0.165`, clamped 0–100;
  `externalPower()` from status bit 1 (USB attached). Read every 5 s.
- **RTC:** registers 18–21; set by NTP like the other boards' RTCs.
- **SD:** Arduino `SD_MMC` 4-bit on the slot-0 pins (wadamesh uses `SD_MMC`
  too).
- **Sound:** a small ES8156 tone player (`audio_es8156.cpp`): the BSP's
  register sequence, Arduino `ESP_I2S` at 44.1 kHz, the amplifier on only while
  a beep plays, volume through the DAC volume register 0x14. `hasVolume` true.
- **GPS, haptics, touch:** none.
- **Power off:** not wired (the power button's 2 s hold does it in hardware).

## 7. Build, scripts, docs

- `boards/tanmatsu.json` (from `tdisplay_p4.json`, `esp32p4_es`), env
  `tanmatsu` (`BOARD_TANMATSU`, `partitions_16mb.csv`), `board::otaSlug()`
  "tanmatsu", `board::name()` "Tanmatsu".
- CI builds it; `scripts/build-upload-monitor.sh` takes `--tanmatsu`/`-T`.
- **Not released yet:** it stays out of `scripts/release.sh` and the flasher
  until it has been tried on hardware (it replaces the launcher, so the
  user decides when to ship it).
- AGENTS.md, docs/BUILD.md, docs/USE.md and the hardware checks in
  docs/TESTING.md.

## 8. Hardware checks (docs/TESTING.md)

Panel up and the right way round (`PANEL_ROTATION`), colours (RGB/BGR), keys
and modifiers, keyboard light, LEDs, backlight levels, battery and charging,
RTC, SD, speaker, the C6 Wi-Fi link, restart, and going back to the launcher
by reflashing it.
