# Attaky Cyber Deck Plus 1.0 — design

Date: 2026-11-01. Status: contributed in PR #6 (WRLS6H05T), reviewed and
amended by the maintainer. Plan: `docs/superpowers/plans/2026-11-01-cyberspace-cyber-deck-plus.md`.

## 1. Goal

A firmware env `cyber_deck_plus` for the Attaky Cyber Deck Plus with every
feature the Mesh Deck has except GPS time, plus a gamepad for navigation, the
Media I/O speaker for beeps and its microSD slot for backups.

## 2. Hardware

Cyber Deck Plus 1.0 = `Core_ESP32_1.0` (the Mesh Deck's Core: ESP32-S3 N16R8,
2.0" ST7789 240x320 mounted 180°, FT6636 touch at 0x38, AW9523 at 0x59 with
D-pad, Select, L1, R2, Power and an RGB LED, BOOT on IO38) +
`Expand_Media-I/O_1.0` + `Control_Keyboard_(Bottom)_1.0` (two AW9523 at
0x5A/0x5B) + `Control_Gamepad_(Bottom)_1.0` + `Power_Standard-Cell_1.0`
(MAX17048 at 0x36). Source: the Attaky datasheet "cyberdeck plus 1.0".

| Part | Pins / address | Notes |
| --- | --- | --- |
| Gamepad, left half | TCA9555 at 0x22, port 0 | D-pad, L1, L2, FN L1/L2; inputs with the chip's pull-ups |
| Gamepad, right half | TCA9555 at 0x26, port 0 | A, B, X, Y, R1, R2, FN R1/R2 |
| NS4168 amplifier | I2S BCLK 17, DATA 18, LRCLK 21 | no enable line; 16 kHz, 16-bit |
| ICS-43434 microphone | WS 9, SD 16, SCK 39, L/R 46 | held parked (unused) |
| microSD | SDMMC CLK 5, CMD 6, DAT0 7 (DAT3 8) | 1-bit SDMMC |

There is no GPS and no LoRa module: IO17/18/21, the Mesh Deck's GPS UART and
SX1262 DIO1, are the amplifier's here, so the Mesh Deck build must never
drive an amplifier on them and this build never starts the GPS. No RTC.

## 3. Design

- **Shared Core.** Both Attaky decks carry the same Core, so its pins and I2C
  code (bus recovery, the AW9523s, FT6636, MAX17048, keyboard scan and
  debounce, LEDs) live once in `src/hal/attaky/` (`pins_attaky_core.h`,
  `attaky_core.{h,cpp}`), compiled for `BOARD_MESH_DECK` and
  `BOARD_CYBER_DECK_PLUS`. `src/hal/mesh_deck/` keeps the GPS and the SX1262
  sleep; `src/hal/cyber_deck_plus/` keeps the gamepad and audio.
- **Gamepad** (`lib/core/src/input/keymap_cyber_deck`, native tests in
  `test/test_cyber_deck_input`): the D-pad is Up/Down/Left/Right on press with
  repeat; A is Select (held: ScreenOff); B and L1 are Back (held: OpenHome),
  on release, and a hold replaces the tap. A release must read twice. The
  halves are told apart, since A and B share bits with Right and Left.
  X, Y, L2, R1, R2 and the FN keys are unmapped.
- **Audio.** `board::beep` queues a tone for an I2S task (core 0, like the
  other boards' audio). The amplifier's lines are always driven by I2S from
  boot; a tone streams with a 10 ms fade in and out, and after 1 s of silence
  the channel is disabled until the next tone, so an idle deck isn't clocking
  the amplifier. A failed start deletes the channel and parks the pins low.
- **Wake.** As on the Mesh Deck: the Core's R2 (or BOOT) toggles the screen;
  `wakeHint()` says R2.
- **Key queue.** One scan can return up to 8 keyboard changes, a repeat and 4
  events each from the Core's buttons and the gamepad; the queue holds all 17.
- **Setup AP:** `Monster-Cyberspace-CyberDeckPlus` (the `-Setup` suffix would
  pass 32 characters). OTA slug `cyber-deck-plus`, a release env with OTA.

## 4. App changes made with this board (all boards)

- **Feed: a pinned "+ New entry" row**, first in the list and selected when
  the Feed opens, so a D-pad and Select alone can post. It goes through
  `ui::typingElsewhere` like the commands menu's New entry (the X4 is sent to
  web config). The empty, loading and offline line sits below it.
- **Connection state with no room open.** Once Wi-Fi is up and an account is
  signed in, the net worker raises the state to reconnecting, and to live once
  the account is ready with no room open; signed out it stays offline. Before,
  only a room's stream set live, so the Feed, Entry, Composer and C-Mail said
  "offline" on a working network. The X4's timed-wake settle timer
  (`power_service`) now starts once bootstrap has the rooms and conversations.
- **Tab opens the entry menu** while typing in an entry.
- **Friends**: Add friend / Remove friend in a message's menu, stored on the
  device (`Settings::friends`, NVS key `friends`, one name per line). Its
  purpose beyond the menu is an open question to the contributor (PR #6).

## 5. Risks

The panel MADCTL, touch mapping and the amplifier's behaviour when its I2S
channel stops and starts need a hardware check (`docs/TESTING.md`). Moving the
Mesh Deck onto the shared Core code needs a regression check on a Mesh Deck.
