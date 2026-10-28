# Monster Cyberspace

Standalone ESP32 firmware client for [cyberspace.online](https://cyberspace.online),
a retro text-only social network. It runs on the Elecrow ThinkNode M9, with
builds for the Seeed Wio Tracker L2, the LilyGO T-Deck Plus, the Elecrow
CrowPanel Advance 3.5" and 5.0" and the LilyGO T-LoRa Pager (not yet tried on hardware), and offers cIRC chat (milestone 1), C-Mail private messages (milestone 2), and a
web config page served by the device. No phone app or companion is needed.

## Install

Download the image for your board from [Releases](../../releases):

- **First install:** `monster-cyberspace-<board>-<version>.bin`, flashed at `0x0`.
  It wipes saved settings.
- **Update:** `monster-cyberspace-<board>-<version>-app.bin`, flashed at
  `0x10000`. It keeps your Wi-Fi and sign-in.

Flash either one with the `flash.sh` attached to each release
(`./flash.sh monster-cyberspace-<board>-<version>.bin /dev/ttyUSB0`; on macOS,
`/dev/cu.usbmodem*` or `/dev/cu.wchusbserial*`), with
[esptool-js](https://espressif.github.io/esptool-js/) in Chrome or Edge, or with
`esptool.py --chip esp32s3 write_flash <address> <file>`.

## Docs

- [Using it](docs/USE.md): setup, buttons, rooms, C-Mail, web config
- [Building](docs/BUILD.md)
- [Testing](docs/TESTING.md): unit tests, hardware checks and the live checklist
- [Design spec](docs/superpowers/specs/2026-10-25-cyberspace-m9-circ-design.md)
  and [plan](docs/superpowers/plans/2026-10-25-cyberspace-m9-circ.md);
  C-Mail [spec](docs/superpowers/specs/2026-10-25-cyberspace-m9-cmail-design.md)
  and [plan](docs/superpowers/plans/2026-10-25-cyberspace-m9-cmail.md)

Contributors and coding agents: read [AGENTS.md](AGENTS.md) first.
