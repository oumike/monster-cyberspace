# Attaky Mesh Deck 1.0 — survey

Approved 2026-10-29, collaboration mode autonomous.

## Hardware

Mesh Deck 1.0 = `Core_ESP32_1.0` (ESP32-S3 N16R8, 2.0" ST7789 240x320 mounted
180°, FT6636 touch at 0x38, AW9523 at 0x59 with D-pad, Select, L1, R2, Power
and an RGB LED, BOOT on IO38) + `Control_Keyboard_(Bottom)_1.0` (48 keys, two
AW9523 at 0x5A/0x5B, an RGB LED per half) + `Expand_LoRa-GPS_1.0` (SX1262,
ATGM336H GPS at 9600 baud on IO17/18) + `Power_Standard-Cell_1.0` (MAX17048 at
0x36). Sources: the Attaky hardware catalog; camillia-mt `hw_mesh_deck.h`;
platy-aprs `pins_mesh_deck.h` and `mesh_deck_io.cpp` (both GPL-3.0, like this
project).

## Features

| Feature | Module | Verdict |
|---|---|---|
| Display | Core | required |
| Touch | Core | required (a Deck can lack the keyboard) |
| D-pad, Select, L1, R2 | Core | required |
| Keyboard | Keyboard | required for typing; touch and the on-screen keyboard without it |
| Notification light (RGB LEDs) | Core, Keyboard | added at the user's request, as platy-aprs |
| Battery | Power | optional |
| GPS time | LoRa/GPS | optional |
| LoRa | LoRa/GPS | dropped (unused; a gateway is a later idea, spec §12) |
| IMU, light sensor | Core | dropped |

No SD slot, buzzer, RTC or keyboard backlight. The Power button's long press
cuts power in hardware, so it is never a UI key.

## Risks

Panel MADCTL and touch mapping need a hardware check. camillia-mt saw a hang
after a warm reset straight out of download mode; power-cycle if the screen
stays dark after flashing. The console is UART0 through a CH340.
