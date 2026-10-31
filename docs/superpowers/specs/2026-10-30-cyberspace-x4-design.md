# Xteink X4 board — design

Status: sections 1–3 approved in conversation 2026-10-30; section 4 (testing
and risks) presented but not yet approved; awaiting written-spec review.
Branch: `x4`.

## 1. Goal

Run Monster Cyberspace on the Xteink X4 as a mostly passive client: an e-paper
screen that shows cIRC, the Feed, C-Mail, Guilds and notifications and keeps
itself up to date, driven by a handful of buttons. Things that need no
typing work on the device: browsing, likes, marking read, joining and
leaving, and canned replies. Free text comes from the web config page, which
acts as a proxy for the device on this board only.

The other boards keep working as they do today. The shared changes
(no-PSRAM paths, the picker, the web config switch) must not change their
behaviour, apart from the new "Web config" setting they all gain.

Hardware reference: `../../rhino-moshi/firmware/include/boards/x4.h`,
`src/panel_x4.cpp`, `src/buttons.cpp`, `src/power.cpp` and the `[x4]` section
of its `platformio.ini`.

## 2. Decisions (from the user)

| Topic | Decision |
|---|---|
| Features shown | cIRC, Feed, C-Mail, Guilds, notifications |
| Button actions | Browse/switch; like; mark read; join/leave rooms and guilds; canned replies |
| Canned replies | Preset phrases (edited in web config) plus the 40-emoji tray |
| Power | Hybrid: live on USB; on battery live while in use, then timed deep sleep with periodic refresh |
| Battery idle period | A setting, X4 only (default 5 min) |
| Architecture | Approach A: the same app, with a no-PSRAM / no-typing board profile. Not a separate app, not a fork |
| Web compose | The web config page sends text directly, X4 only. No page password required; the box is always shown |
| Key-press rule | Web compose is an explicit exception to "every write comes from a key press", for the X4 only. AGENTS.md is updated to say so |
| Web config switch | A new device setting, "Web config: on/off", on every board |
| Building | Only `pio run -e x4`, and only when the user says. No all-env builds without asking |

## 3. Hardware (from rhino-moshi `x4.h`)

| Part | Detail |
|---|---|
| SoC | ESP32-C3: one RISC-V core, ~400 KB SRAM, **no PSRAM**, 16 MB flash |
| Console | The C3's native USB-CDC (`ARDUINO_USB_MODE=1`, `ARDUINO_USB_CDC_ON_BOOT=1`) |
| Panel | 4.26" GDEQ0426T82, 800x480 native, black and white, GxEPD2 `GxEPD2_426_GDEQ0426T82`. SPI: SCLK 8, MOSI 10, CS 21, DC 4, RST 5, BUSY 6, MISO 7 (shared). Used portrait (rotation 3): 480x800 |
| Buttons | ADC1 on GPIO1: Back, Confirm, Left, Right. ADC2 on GPIO2: Up, Down. Power on GPIO3 (active low, pull-up). Thresholds: `{60500, 49000, 33000, 12000}` and `{50500, 18000}` in 16-bit ADC space |
| Key positions | Up/Down on the right edge; Back, Confirm, Left, Right left to right below the screen |
| Battery | GPIO0 through a 2:1 divider; USB detect GPIO20 (high on USB) |
| Power latch | GPIO13: HIGH keeps the battery connected, LOW cuts it |
| SD | CS 12 on the panel's SPI bus |
| Not fitted | Touch, keyboard, GPS, RTC, buzzer, lights, LoRa |

## 4. Design

### 4.1 Board and build

- Env `x4`: `board = esp32-c3-devkitm-1`, 16 MB flash,
  `board_build.partitions = partitions_16mb.csv` (OTA slots and NVS past them,
  as on the other boards). GxEPD2 with `GxEPD2_DISPLAY_CLASS=GxEPD2_BW` and
  `GxEPD2_DRIVER_CLASS=GxEPD2_426_GDEQ0426T82`.
- Added to `RELEASE_ENVS` in `scripts/release.sh`, to CI, to
  `scripts/build-upload-monitor.sh` (flag `-X`) and to
  `tools/set_app_version.py` as the other boards are. `board::otaSlug()`
  returns `x4`.
- HAL in `src/hal/x4/` behind `src/hal/board.h`, nothing included elsewhere:
  - Panel: GxEPD2 in portrait. `pushRows` packs to 1 bit (as the T-Deck Pro
    does), and refresh timing comes from `ui/eink` in `lib/core`.
  - Buttons: a sampling task (5 ms, 50 ms debounce, 700 ms hold) queues
    portable keys for `board::pollKey`. The ladder decoding is portable
    (`lib/core/src/input/keymap_x4`) so it is tested on the host.
  - Battery percent and `externalPower()` from GPIO0 and GPIO20; SD on CS 12.
  - Flags: `eink()` true, `hasKeys()` true, `hasKeyboard()` false,
    `hasTouch()` false, `canType()` false (new, §4.3), `webCompose()` true
    (new, §4.4), no GPS, RTC, buzzer or lights.

### 4.2 Shared no-PSRAM changes

- Renderer: when PSRAM is absent, keep a 1-bit frame (480x800/8 = 48 KB) in
  internal RAM instead of the RGB565 one. Colours are reduced to black or
  white as they are written, using the paper theme the T-Deck Pro already
  uses. Boards with PSRAM keep today's path unchanged.
- Allocations that ask for `MALLOC_CAP_SPIRAM` (TLS, the SSE buffer, caches)
  go through one helper that falls back to internal RAM.
- Task creation goes through a helper that pins to the requested core when it
  exists and to core 0 otherwise (the C3 has only core 0).
- Smaller limits when there's no PSRAM: the SSE buffer, message, feed and
  thread caches. The spell dictionary is left out of the X4 build (nothing is
  typed on the device).
- At most two TLS connections at once: the open screen's stream and one for
  requests. On the X4, notifications are polled on the requests connection
  instead of streamed.
- The heap's free and low-water marks are logged on the serial console.

### 4.3 Buttons and the canned picker

- Keys: Up/Down move the selection or scroll; Left/Right go to the previous
  or next tab, room or page; Confirm opens; holding Confirm opens the item's
  menu (Like, Reply, Mark read, Join/Leave as they apply); Back goes back,
  holding Back goes Home. Power: a tap sleeps the screen, a hold powers off.
- Home lists cIRC, Feed, C-Mail, Guilds, Notifications and Settings, using
  the existing screens.
- New `board::canType()` (false on the X4, true elsewhere). When it is false,
  anything that would open a text field opens the canned picker instead: the
  user's phrases first, then the 40 tray emoji. Confirm sends the picked item;
  Back cancels. Actions that need free text (a new thread's title, a new
  mail's subject) are hidden on the device.
- Phrases: up to 10, each up to 80 characters, edited in web config, saved in
  NVS and included in the backup YAML. Portable list model in `lib/core`.

### 4.4 Web compose (X4 only)

- New `board::webCompose()` (true only on the X4). When true, the web config
  page shows a "Send" section, always, with no password required.
- It shows what is open on the device (room, conversation, post or thread)
  and sends the text there through the same send path the device uses.
  When the open screen takes no text, the section says so.
- It can also start a new C-Mail (recipient, subject, body) or a new guild
  thread (guild, title, body).
- Each send is one HTTP request from the page; there are no automatic
  retries. AGENTS.md's key-press rule gains this X4 exception.

### 4.5 Web config switch (all boards)

- New setting "Web config: on/off" (default on) in device Settings, web
  config and the backup YAML. Off: the page is not served on the joined
  Wi-Fi network. The setup AP always serves it, so it can't lock you out.

### 4.6 Passive display

- New messages, posts and notifications redraw the open screen as they
  arrive, no more than one partial refresh every 2 s, with a full refresh
  from `ui/eink`'s schedule to clear ghosting.

### 4.7 Power (X4)

- On USB: always live.
- On battery: live while in use. After the idle period (setting, X4 only,
  default 5 min) it draws a status line ("Sleeping — updated hh:mm") and
  enters timed deep sleep with the latch held HIGH, waking every 15 min by
  default (web config setting).
- Timed wake: join Wi-Fi, fetch the open screen and the notification count
  (reads only), redraw once, sleep again. A button press wakes it fully;
  GPIO wake on the power button, as rhino-moshi does.
- Critical battery (under 3.40 V): draw an "empty" screen and power off with
  the latch LOW.
- The wake and sleep decisions are a portable model in `lib/core`, tested on
  the host; `src/app/power_service.cpp` carries them out.

## 5. Testing and risks

- Host tests (TDD) for: the button ladder decoding, the phrase list (limits,
  NVS form, backup YAML), the picker, the power decisions, web-compose
  request parsing and target selection, and the web config switch rule.
- Never call the live API; web compose is tested against the fixtures.
- Builds: `pio run -e x4` only when the user says; one build of the other
  envs before merging, also only when the user says.
- `docs/TESTING.md` gains X4 hardware checks: orientation and refresh,
  button labels against the keys, free heap with two streams open, sleep,
  timed wake and power off, web compose.
- Risk: RAM. Fallbacks, in order: smaller caches, then one stream at a time.

## 6. Docs

AGENTS.md (layout, the key-press exception, commands), `docs/BUILD.md`,
`docs/USE.md` (an X4 section), `docs/TESTING.md`.
