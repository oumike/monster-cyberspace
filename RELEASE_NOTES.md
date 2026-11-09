### Monster for Cyberspace 2.0.0

### A new UI, drawn with LVGL

- Every board but the Xteink X4 now draws its screens with [LVGL](https://lvgl.io) 9.5. The screens, keys, touch gestures, themes and Terminus fonts are the same as before; under the hood, each screen is built once and updated in place instead of being redrawn cell by cell.
- Touch scrolling and taps keep their old rules: the same drag steps, slop and long press, in lists, logs, the Feed, the drawer and every button grid.
- Boards without PSRAM (the M5Stack Cardputer ADV) draw in thin strips with a lean LVGL, inside the same memory as before. If a screen ever runs out of memory, the old one stays up and the new one is retried.
- The Xteink X4 keeps the previous UI for now.

### Settings

- Settings follows your text size, like every other screen. On touch screens its buttons drop to fewer columns, and leave off a value that won't fit beside its label.
- The Settings page scrolls even when its status lines fill the screen.
- Its header now matches every other screen's.

### Seeed Wio Tracker L2

- Fixed the panel smearing and screens drawn in the wrong place.
- The UI stays clear of the case's bezel, so the header's top is no longer cut off.
- Large text is no longer offered: the screen is too small for it. A saved Large becomes Comfortable.

### Fixes

- The Feed no longer crashes on first open (seen on the M9).
