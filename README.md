# Monster Cyberspace

Standalone ESP32 firmware client for [cyberspace.online](https://cyberspace.online),
a retro text-only social network. It runs on the Elecrow ThinkNode M9, with
builds for the Seeed Wio Tracker L2, the LilyGO T-Deck Plus, the Elecrow
CrowPanel Advance 3.5" and 5.0", the LilyGO T-LoRa Pager and the LilyGO T-Deck Pro (not yet tried on hardware), and offers cIRC chat (milestone 1), C-Mail private messages (milestone 2), and a
web config page served by the device. No phone app or companion is needed.

## Install

Download `monster-cyberspace-<board>-<version>.bin` for your board from
[Releases](../../releases) and flash it at `0x0` with the `flash.sh` attached to
each release (`./flash.sh monster-cyberspace-<board>-<version>.bin /dev/ttyUSB0`;
on macOS, `/dev/cu.usbmodem*` or `/dev/cu.wchusbserial*`). Installs keep your
Wi-Fi, sign-in and preferences: they live past the end of the image.

Boards installed before over-the-air updates arrived kept their settings at
`0x9000`. `flash.sh` skips that area and the firmware moves them on its first
boot; other tools ([esptool-js](https://espressif.github.io/esptool-js/),
`esptool.py --chip esp32s3 write_flash 0x0 <file>`) write over it, so for those
take a backup first (Settings → Back up to SD card, or web config).

After that, updates come over the air (Settings → Firmware, or web config). The
`-ota.bin` files are for that and are not flashed by hand.

## Docs

- [Using it](docs/USE.md): setup, buttons, rooms, C-Mail, web config
- [Building](docs/BUILD.md)
- [Testing](docs/TESTING.md): unit tests, hardware checks and the live checklist
- [Design spec](docs/superpowers/specs/2026-10-25-cyberspace-m9-circ-design.md)
  and [plan](docs/superpowers/plans/2026-10-25-cyberspace-m9-circ.md);
  C-Mail [spec](docs/superpowers/specs/2026-10-25-cyberspace-m9-cmail-design.md)
  and [plan](docs/superpowers/plans/2026-10-25-cyberspace-m9-cmail.md)

Contributors and coding agents: read [AGENTS.md](AGENTS.md) first.

## License

[GPL-3.0](LICENSE). Board code is ported from the GPL-3.0 projects camillia-mt
and platy-aprs. The cell fonts are derived from Terminus Font under the SIL Open
Font License 1.1 ([src/ui/fonts/OFL-Terminus.txt](src/ui/fonts/OFL-Terminus.txt)),
and the emoji are rendered from Noto Emoji under the same licence
([src/ui/fonts/OFL-NotoEmoji.txt](src/ui/fonts/OFL-NotoEmoji.txt)).
Cyberspace itself is a separate service with its own terms; this project is not
affiliated with it.
