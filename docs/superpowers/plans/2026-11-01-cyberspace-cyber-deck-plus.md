# Attaky Cyber Deck Plus 1.0 — plan

Spec: `docs/superpowers/specs/2026-11-01-cyberspace-cyber-deck-plus-design.md`.
The first version came as PR #6 (one commit); the maintainer review on
2026-11-01 asked for the tasks below, done on the same branch.

## 1. Identity

- firmware-name: Monster for Cyberspace
- build-SKU: Cyber Deck Plus 1.0 (`Core_ESP32_1.0` + `Expand_Media-I/O_1.0` +
  `Control_Keyboard_(Bottom)_1.0` + `Control_Gamepad_(Bottom)_1.0` +
  `Power_Standard-Cell_1.0`)
- env `cyber_deck_plus`, OTA slug `cyber-deck-plus`, boot banner
  `[INFO] [APP] Monster for Cyberspace v<RC_VERSION> on Cyber Deck Plus 1.0`

## 2. Tasks

- [x] Board HAL, gamepad key map, Media I/O audio and SD, env (PR #6)
- [x] Feed New entry row, connection state, Tab in Entry, Friends (PR #6)
- [x] Feed New entry row checks `ui::typingElsewhere` like the menu's
- [x] `wakeHint()` names R2, the Core button that wakes it
- [x] Connection state stays offline while signed out; the X4 settle
      reasoning documented in `power_service`
- [x] Audio: the I2S channel idles after 1 s of silence; a failed start
      deletes the channel; the task on core 0
- [x] Shared Attaky Core code in `src/hal/attaky/`, used by the Mesh Deck too
- [x] Key queue sized for a full scan of all three sources
- [x] Comments fixed (SD, SPI, cores, gamepad mapping); unused `kGp*` removed
- [x] Native tests: `test/test_cyber_deck_input`
- [x] Wiring: `scripts/release.sh`, CI, `scripts/build-upload-monitor.sh`
      (`--cyber-deck-plus`/`-K`), `site/index.html`, monster-cyberspace-web
- [x] Docs: this plan and spec, AGENTS.md, BUILD.md, TESTING.md, USE.md
- [ ] Friends: the contributor's intent (what it should do, backup, a cap)
- [ ] Hardware checks in `docs/TESTING.md` (Cyber Deck Plus, the Mesh Deck
      regression line, the Feed row and connection state, the X4 timed wake)

## 3. Milestones (each needs device evidence)

- M0: `pio run -e cyber_deck_plus` and `-e mesh_deck` build; the other envs and
  `pio test -e native` stay green.
- M1 display and touch: banner in the log, splash upright, taps under the finger.
- M2 input: Core buttons, keyboard and gamepad as in TESTING.md.
- M3 audio: clean beeps, no noise idle or when the channel stops and starts.
- M4 storage and power: backup and restore on SD, battery %, charging on USB.

## 4. Toolchain

The repo's `[env]` platform (pioarduino, Arduino 3.x), esp32-s3-devkitc-1,
qio_opi, 16 MB, `partitions_16mb.csv`. Console on UART0 through the CH340
(`ARDUINO_USB_CDC_ON_BOOT=0`).
