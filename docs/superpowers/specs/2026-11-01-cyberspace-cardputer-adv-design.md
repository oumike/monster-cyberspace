# Cardputer ADV — design

Date: 2026-11-01. Status: approved in chat, awaiting spec review.

## 1. Goal

A firmware env `cardputer_adv` for the M5Stack Cardputer ADV that does cIRC
rooms and nothing else Cyberspace-side. Success: it builds, boots, is set up
through web config, signs in, and a user can open the room list, read a room
and send to it from the keyboard. No flashing until the user says so.

User decisions:

- **Unread compiled out.** It is not in this build at all, to save flash and
  RAM. (C-Mail, the Feed, People and Guilds were added on 2026-11-01 at the
  user's request: a conversation reuses the room's two streams, the others
  load over plain requests, so none adds a TLS connection; replies are
  capped at 30, `RC_MAX_REPLIES`, as on the X4, the guild browse list at 40
  and members at 60, `RC_MAX_GUILDS`/`RC_MAX_MEMBERS`.)
- **6×12 cells** (the existing Compact font): 40 columns × 11 rows.
- **Single app partition, no OTA, no mascot partition.** Updates come over USB
  or the browser flasher. Fluffy is always the default (mascot is low
  priority).
- **Extras kept:** battery level, speaker beep on mentions, settings backup
  to SD. **Dropped:** spell check, emoji tray and emoji glyphs.

## 2. Hardware

ESP32-S3FN8 in a Stamp-S3A: 8 MB flash, **no PSRAM**. Pins from the M5 schematic
and M5Unified (cross-checked in developing-today/hardware-doc,
`devices/m5stack/cardputer-adv/pinouts-and-buses.md`) and camillia-mt's
`src/hal/hw_cardputer.h`:

| Part | Pins / address | Notes |
| --- | --- | --- |
| ST7789V2 240×135 | SPI3: SCK 36, MOSI 35, CS 37, DC 34, RST 33; no MISO | offsets X 53, Y 40 (camillia-mt), inverted, 40 MHz |
| Backlight | GPIO 38, LEDC | also enables the `BL_3V3` rail |
| TCA8418 keyboard | I²C SDA 8, SCL 9, addr 0x34, INT 11 | the Pager's TCA8418 path |
| ES8311 codec | I²C 0x18; I²S BCLK 41, LRCK 43, DOUT 42 | **no MCLK line**: the codec must clock from BCLK |
| Battery | ADC1, GPIO 10 | through a divider |
| microSD | SPI SCK 40, MOSI 14, MISO 39, CS 12 | shared with the CAP header (unused) |
| BOOT button | GPIO 0 | unused (no boot gesture, AGENTS.md) |

Not used: BMI270 IMU, IR, WS2812, the CAP header, the microphone. No touch, GPS
or RTC; time comes from NTP.

## 3. Feature flags

New compile-time flags in one header, `lib/core/src/features.h`, each
defaulting to 1 so every existing board builds as it does today:

`RC_FEAT_CMAIL`, `RC_FEAT_FEED`, `RC_FEAT_GUILDS`, `RC_FEAT_PEOPLE`,
`RC_FEAT_UNREAD`, `RC_FEAT_SPELL`, `RC_FEAT_EMOJI`, `RC_FEAT_OTA`,
`RC_FEAT_MASCOT`.

The `cardputer_adv` env sets all nine to 0. Rules:

- A feature's own `.cpp` files wrap their whole body in `#if RC_FEAT_X`, the
  same way HAL files use `#if defined(BOARD_X)`. No `build_src_filter`.
- Everything else guards only its **wiring points**: the router's screen
  switch and `Id` handling, Home tiles and Home's key shortcuts, router hot
  keys, workers and services started in `main`/`ui_task`, net-worker channels
  and subscriptions, web config sections and endpoints, Settings items, and
  the room screen's links into other features (profiles, C-Mail).
- `router::Id` keeps every value (it's an enum; values are cheap). Going to a
  compiled-out screen is impossible because nothing wires to it.
- Portable choices that depend on the flags (Home's tile list, which Settings
  items show) live in `lib/core` and get native tests with flags on and off.
- **Spell off:** the `gen_spell_dict.py` pre-script writes an empty header
  when `RC_FEAT_SPELL=0`, and the spell strip is not drawn.
- **Emoji off:** no tray and no `emoji_cells.h` bitmaps. Text widths stay the
  same (`ui/text_width.h` still counts a wide emoji as two cells), and the
  renderer draws each wide emoji as a placeholder glyph in its two cells.
- **OTA off:** no `ota_service`, no update items in Settings or web config,
  and the release ships no `-ota.bin` or `.sig` for this board.
- **Mascot off:** no `mascot_store`, no `/api/mascot`, no Mascot section in web
  config; the splash and screensaver use the built-in Fluffy.

## 4. Board HAL — `src/hal/cardputer_adv/`

Follows the existing HALs; every file is gated `#if defined(BOARD_CARDPUTER_ADV)`.

- **Panel:** the shared `st7789` driver through its `Config` (size 240×135
  landscape, offsets, MADCTL, inversion). The exact MADCTL waits for the
  hardware check, like the Pager's.
- **Keyboard:** the TCA8418 driver from the Pager, with a new portable
  `lib/core/src/input/keymap_cardputer_adv` turning TCA8418 key events into
  portable keys. The 4×14 matrix layout and the Fn layer come from M5Cardputer
  1.1.x's TCA8418 reader. Fn plus `;` `.` `,` `/` are the arrows, as printed on
  the keys; Esc (the `` ` `` key) is Back; Enter selects. Native tests cover
  the table, the shift and Fn layers and the arrows.
- **Speaker:** the Wio/Pager `es8311` driver gains a "clock from BCLK" option
  (register 0x01 and 0x02 as M5Unified programs them for the ADV), used only
  by this board.
- **Battery:** ADC on GPIO 10 with the divider factor, mapped to a percentage
  like the other ADC boards.
- **SD:** the shared SD access, mounted per operation, for settings backup.
- **Flags:** `hasKeyboard`, `canType` true; `hasTouch`, `canRotate`, `eink`,
  `hasNotifyLight` false. `otaSlug()` is unused (OTA off).
- **Text size:** a new HAL hook `board::fixedTextSize(TextSize&)` returns true
  with `Compact` on this board and false elsewhere. When it's true, the UI
  always uses that size (splash and screensaver included), and Settings and
  web config hide the text-size choice. This replaces, for this board only,
  `RC_LOW_MEM`'s "Compact never" rule, which exists because the X4's big panel
  can't fit a Compact grid; a 40×11 grid is tiny.
- **Splash and screensaver** must fit 40×11: the 6-row art, bubble, name,
  version and boot line. The splash layout checks the rows it has and drops
  the version line, then the bubble, if they don't fit.

## 5. Memory

- `RC_LOW_MEM=1` as on the X4: strip rendering (`ui/renderer`, `sys/mem`), the
  smaller TLS buffers and the smaller UI stack.
- `RC_CHAT_CAP=150`. `RC_MAX_REPLIES` is irrelevant with Feed off.
- The S3 has about 320 KB of usable internal RAM, more than the X4's C3, so
  TLS with Wi-Fi up is expected to fit. The build prints free heap after
  sign-in on the console so the hardware check can confirm it.

## 6. Flash layout — `partitions_8mb.csv`

| Name | Type | Offset | Size |
| --- | --- | --- | --- |
| factory | app/factory | 0x10000 | 0x7C0000 |
| nvs | data/nvs | 0x7D0000 | 0x20000 |
| coredump | data/coredump | 0x7F0000 | 0x10000 |

No `otadata`, no second app, no `mascot`. NVS sits past the app, as in
`partitions_16mb.csv`, because a factory image written at 0x0 fills
everything up to the app's end (0x9000 included); there settings survive a
reflash without erase. `nvs_migrate` finds no `nvs_old` and does nothing.

## 7. Build, release and flasher

- Env `cardputer_adv` in `platformio.ini`: `esp32-s3-devkitc-1`, 8 MB QIO
  flash, no PSRAM flags, USB CDC on boot, `-DBOARD_CARDPUTER_ADV`,
  `RC_LOW_MEM`, `RC_CHAT_CAP=150`, the nine `RC_FEAT_*=0`.
- `scripts/build-upload-monitor.sh` gets a short flag (`-C`), and `-B` builds it.
- `RELEASE_ENVS` gains it; the release workflow and `scripts/release.sh`
  skip the OTA image and signature for envs with OTA off (a list in
  `release.sh`). `tools/set_app_version.py` and CI's checks know it.
- The browser flasher gets a label, "M5Stack Cardputer ADV", in `LABELS`
  (firmware repo `site/` and monster-cyberspace-web). Its Make your Fluffy and
  Phrases buttons don't apply to this board; the flasher hides them when it is
  picked.
- Docs: AGENTS.md (board list, commands, layout), docs/BUILD.md, docs/USE.md
  (keys), docs/TESTING.md (an "Open hardware checks" entry).

## 8. Testing

- Native: the keymap, Home's tile list and Settings items with the flags off,
  the splash fitting 40×11, and the placeholder for wide emoji with emoji off.
- `pio test -e native` green; `pio run -e cardputer_adv`; the full
  `scripts/build-upload-monitor.sh -B` shows every other board still builds.
- Hardware check (TESTING.md, by the user): panel orientation and offsets,
  every key, backlight, battery, beep, SD backup, sign-in and free heap, a room
  read and send.

## 9. Out of scope

C-Mail, Feed, Guilds, People and Unread on this board (later, one flag at a
time), OTA, the custom mascot, the IMU, IR, the RGB LED, the microphone and
the LoRa CAP.
