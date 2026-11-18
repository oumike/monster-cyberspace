### Monster for Cyberspace (next release)

### New boards

- **M5Stack Tab5**, with or without the Tab5 Keyboard, in the browser flasher. It boots, draws and takes touch on a unit with the ST7121 panel; the ST7123 and ILI9881C panels, and most of the rest, are not yet tried on hardware. Wi-Fi goes through its ESP32-C6's factory image.
- **M5Stack CardputerZero** (Linux): an app for its launcher, as `monster-cyberspace-cardputer-zero-v2.4.0_arm64.deb` on this release. Install it with `sudo apt install ./monster-cyberspace-cardputer-zero-v2.4.0_arm64.deb`. It has every feature but over-the-air updates, and runs on hardware.

### Changes

- Touch boards: the status bar's `‹` now answers a tap anywhere on the title, and `⋯` anywhere on the status icons, so both are easier to hit.
- Signing in from the web config page while the device shows setup now leaves setup for Home.
