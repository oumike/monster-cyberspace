# Rhino Cyberspace — milestone 1: cIRC + web config on the ThinkNode M9

Date: 2026-10-25 · Status: design approved in brainstorming, awaiting spec review

## 1. Intent

A standalone firmware client for [cyberspace.online](https://cyberspace.online)
that other Cyberspace members can flash and use, not only its author. Over time
it covers all of Cyberspace (cIRC, C-Mail, Feed, Notifications, and the rest)
on several boards. This milestone delivers **cIRC chat** and a **minimal web
config page** on the **Elecrow ThinkNode M9**.

### Success criteria (milestone 1)

A member with an M9 and a verified Cyberspace account can:

1. Flash the firmware and, with no Wi-Fi saved, see on screen how to reach the
   setup page: the access point's name (open by default) and
   `http://192.168.4.1`.
2. On that page, enter Wi-Fi details and their Cyberspace email and password.
   The device logs in, keeps only the refresh token, and joins their Wi-Fi.
3. Open the room list, enter a room, read its history, and receive new
   messages live.
4. Send plain messages and `/commands`, and get a clear banner when the server
   rejects one.
5. Scroll back through history, act on a message (reply, reveal, show its link,
   delete own, report), and see who is in the room.
6. Appear in the room's who's-here list while in it, and drop out when they
   leave or the screen turns off.
7. Be alerted, by buzzer and banner, to `@mentions` and to activity in other
   rooms, without having those rooms open.
8. Keep running for longer than the ~1 h login-token lifetime and through
   Wi-Fi dropouts, without rebooting or losing messages.

### Out of scope for milestone 1

C-Mail, Feed, Notifications screen, guilds, profiles, search, jukebox playback,
images (shown as links), signed releases or OTA, web flasher, other boards, and
any LoRa feature (see §12).

## 2. Constraints

- **API terms** (api.cyberspace.online/docs): personal clients that a human
  drives are allowed. Bots, scraping, redistribution, and feeding AI systems are
  not. Every write in this firmware comes from a key press. The device mirrors
  the documented rate limits client-side.
- **Development constraint:** the assistant building this does not call the
  live API with a real account (the terms ban LLM-driven agents). Code is tested
  against fixtures and a fake API. Live testing is done by the human (§10.3).
- **No captive portal.** The setup AP serves the page at a fixed address. There
  is no DNS hijacking and no OS portal detection.
- **Hardware:** ESP32-S3R8, 16 MB flash, 8 MB octal PSRAM; 2.4" ST7789
  240×320 used landscape (320×240); STC8H keyboard controller on I2C. The pin
  map and key protocol are as verified in camillia-mt `src/hal/hw_m9.h` and
  rhino-moshi `firmware/include/boards/m9.h`.
- The M9 keyboard reports no key releases, so the design uses no chords.
  Ctrl arrives as its own key code.

## 3. Toolchain and dependencies

- PlatformIO, platform pioarduino `55.03.312` (Arduino core 3.3 / ESP-IDF 5.5),
  the same as rhino-moshi, which runs natively on Apple Silicon.
- Display: a direct ST7789 driver with a full-frame RGB565 canvas in PSRAM,
  pushing only the dirty row bands (ported from rhino-moshi `panel_m9.cpp`).
  *(Amended during planning; this replaced LovyanGFX.)*
- JSON: ArduinoJson 7, using filters to keep only the fields each screen needs.
- HTTP(S) client: ESP-IDF `esp_http_client` with the IDF certificate bundle.
- HTTP server: ESP-IDF `esp_http_server`. mDNS: ESP-IDF `mdns`.
- Settings: NVS (`Preferences`).
- Environments: `m9` (firmware), `native` (host unit tests), and `m9_fake`
  (firmware with the fake API, §10.2).

## 4. Architecture

Two FreeRTOS tasks communicate only through queues:

- **UI task (core 1):** input, screens, drawing. It never blocks on I/O.
- **Network task (core 0):** Wi-Fi, HTTPS, the SSE stream, auth, the background
  watcher, and the web config server.
  Each open SSE stream (at most two: room messages and room presence) runs in
  its own small reader task on core 0, and the chat store is shared under a
  mutex. *(Amended during planning.)*

Commands flow UI → net (`SendMessage`, `OpenRoom`, `LeaveRoom`,
`LoadOlder`, `DeleteMessage`, `FlagMessage`, `MarkRead`, `Login`, …).
Events flow net → UI (`RoomChanged`, `RoomListUpdated`, `PresenceUpdated`,
`ConnState`, `Banner`, `MentionAlert`, `AuthState`, …).

### Modules

| Module | Responsibility | Depends on |
|---|---|---|
| `hal/m9/` | Panel init (LovyanGFX ST7789, backlight PWM on LEDC ch 7, active low), keyboard driver (USB-pad release, production/early register detection), VEXT rail, battery ADC, buzzer | Arduino, LovyanGFX |
| `hal/board.h` | Board-neutral interface: `display()`, `pollKey()`, `battery()`, `beep()`, `backlight()` | — |
| `input/` | Per-board keymap: raw code → `Action` (`Up/Down/Left/Right/Select/Back`, `OpenCirc/OpenHome/OpenCmail/OpenFeed`, `Palette`, `ClearLine`, `ScreenOff`, `Char(c)`, `Backspace`) | `hal` |
| `ui/grid` | Character-cell framebuffer, dirty-row redraw, 6×12 and 8×16 bitmap fonts (Latin-1 + box drawing + a small symbol set), theme colours, nick-colour hash | `hal` |
| `ui/widgets` | `LogView` (wrap with hanging indent, scrollback, selection), `InputLine` (cursor editing, 2,048-character cap), `List`, `Drawer`, `Menu`/`Palette`, `Banner`, `StatusBar` | `ui/grid` |
| `screens/` | `Splash`, `Setup`, `Home`, `RoomList`, `Room`, `ComingSoon` | widgets, `store`, queues |
| `store/chat` | Per-room message ring in PSRAM (cap 1000), upsert by id, read markers, mention flags | — |
| `store/settings` | NVS: Wi-Fi list, email, refresh token, text size, drawer open, buzzer, page password, optional setup-AP password (empty = open AP), time zone | NVS |
| `chat/source` | Abstract chat source interface the screens use (open/close room, send, older, delete, flag, presence). The milestone-1 implementation is `CsCircSource` | — |
| `cs/auth` | Login, refresh, token lifetime, verification resend | `net/https` |
| `cs/circ` | REST: room list, history, send, delete, flag, read, users, presence | `net/https` |
| `cs/notify` | Unread count, `chat_mention` list, mark read | `net/https` |
| `cs/ratelimit` | Client-side mirror of the documented per-minute, hourly and daily write limits | — |
| `net/https` | One kept-alive TLS connection to `api.cyberspace.online`; JSON in/out; error mapping | IDF |
| `net/sse` | RTDB Server-Sent Events: line parser, `put`/`patch`/`keep-alive`/`cancel`/`auth_revoked`, reconnect with backoff | IDF |
| `net/wifi` | STA with saved networks, setup AP (open, or WPA2 if a password is configured), mDNS `rhino-cyberspace.local`, SNTP | IDF |
| `web/` | Config pages and JSON endpoints | `esp_http_server`, settings, `cs/auth` |

Each module has a header that states its contract. Files stay small and focused.

## 5. Navigation and controls (M9)

Dedicated buttons work from anywhere and close whatever is open first.

| Button | Action |
|---|---|
| Messages (far left) | cIRC: the last room. Pressed again in a room, it opens the room list |
| Home | Home dashboard |
| Fn under Home | C-Mail ("coming soon" in M1) |
| Fn under Back | Feed ("coming soon" in M1) |
| Map | Unassigned (undecided) |
| Back | Up one level, or closes a menu or drawer. It never deletes text |
| Ctrl | Command palette: slash commands, "Text size: Comfortable/Compact" |
| Back/Del hold (0x89) | Clear the input line |
| Centre hold (0xA3) | Screen off; with the screen off, screen on |

**Wake:** only holding the d-pad centre wakes the screen. A centre **tap**
while the screen is off shows the **screensaver** for 10 s: a read-only view
of the last few messages in the current room, from what's already stored (no
network, no presence), then dark again. A hold during the screensaver wakes
fully. Other keys pressed while the screen is off or on the screensaver are
discarded, which keeps pocket presses from sending anything.

**Lists:** Up/Down select, Centre or Right open, Left goes back.

**Room view:** the keyboard always types into the input line.

- Centre or Enter with text sends it. Centre on an empty line opens the room
  menu: who's here, mute/unmute, mark read, text size, leave.
- Up/Down scroll the log, highlighting one message. Up at the top loads older
  history (`GET /v1/circ/:room?before=`). Down past the newest returns to
  typing.
- Centre on a highlighted message opens the message menu: reply (`@name `),
  show in full or reveal a spoiler, show a link as text and a QR code, delete
  (own messages, with confirmation), report (with confirmation and an optional
  reason).
- Left/Right move the text cursor. **Right with the cursor at the end of the
  line toggles the who's-here drawer.** In the drawer, Up/Down pick a person,
  Centre opens their menu (mention, mute/unmute), and Left or Back closes it.
  The drawer state persists across rooms.

## 6. Screens

**Status bar** (all screens): the room or screen name, member count, a
connection marker (● live, ◌ reconnecting, ✕ offline), Wi-Fi signal, battery
and clock.

**Room view: two text sizes**, toggled from the palette or room menu and
saved.

- **Comfortable:** 8×16 font, 40×15 cells. Lines read as `nick message`, with
  wraps aligned under the message. A dim `── 14:05 ──` divider appears when the
  minute changes. There is no hint row.
- **Compact:** 6×12 font, 53×20 cells. camillia-mt IRC style:
  `14:02 nick: message`, with the time in a blue column on the left and wraps
  continuing under the message text. A date divider appears when the day
  changes. A dim hint row at the bottom changes with context.
- The column and row counts come from the panel size and font, never
  hard-coded.

**Drawer:** overlays the right side of the log, which does not reflow. It is
~240 px wide in Comfortable and ~180 px in Compact. It shows "here · N" and
nicks in their colours, with `z` for idle users (`lastActivity` older than
`idleAfterMs`).

**Rendering rules:**

- Nick colours use the website's formula: hue = 32-bit FNV-1a of the
  lower-cased username, mod 360. Lightness is set for a dark background. Your
  own nick is gold.
- Muted users' messages are hidden, using `mutedUsersByRoom` from settings,
  shared with the website.
- `isAction` lines render as `* nick text`. Dice, 8-ball and fortune results
  render with their answer fields.
- `style: "art"`: the content is base64-decoded and drawn verbatim (80×25 max;
  clipped with a "show in full" option). Other styles are ignored, except
  `spoiler`, which is masked until revealed.
- `deleted: true` renders as a dim `[deleted]`. Image, GIF and audio
  attachments render as `[img]`, `[gif]` and `[song] artist – title`.
  Content equal to the attachment URL is not printed twice.
- A message that mentions you (`@yourname`, case-insensitive) is tinted until
  it has been on screen.

**Home dashboard:** the signed-in user, Wi-Fi network and IP, the settings-page
address, unread mentions, rooms with new activity, and battery. Its last
entry is **Factory reset…**, which asks for confirmation (Select on
"Erase everything", default "Cancel") and then does the same as the web
config factory reset.

**Setup screen** (no Wi-Fi saved: first boot, or after a factory reset; there is no boot-time gesture): the AP name,
which includes the board (`Rhino-Cyberspace-<BOARD>-Setup`, so
`Rhino-Cyberspace-M9-Setup` on this board, from the board's `BOARD_NAME`),
and `http://192.168.4.1`. The AP is **open by default**. If a setup-AP
password has been set in web config, the AP uses WPA2 with it, and the screen
says a password is required without displaying it. Once on Wi-Fi it
also shows `http://rhino-cyberspace.local` and the IP. A fallback "Log in on
device" form takes email and password on the M9 keyboard.

## 7. Data flow

### 7.1 Auth

- **Login** (from the web page or the device): `POST /v1/auth/login`. The
  device stores `refreshToken` and `email` in NVS and keeps `idToken` and
  `rtdbUrl` in RAM. The password is never stored or logged.
- **Refresh:** `POST /v1/auth/refresh` five minutes before the `idToken`
  expires (read from its JWT `exp`), and after any `401`. A rejected refresh
  signs the device out and clears the token.
- `403 EMAIL_NOT_VERIFIED` offers `POST /v1/auth/resend-verification`.

### 7.2 Opening a room

1. `GET /v1/circ/:room?limit=50` loads history into `store/chat`.
2. Open SSE:
   `GET {rtdbUrl}/chat_messages/:room.json?auth=<idToken>&orderBy="timestamp"&startAfter=<newest ts>&limitToLast=1000`
   with `Accept: text/event-stream`.
3. `POST /v1/circ/:room/presence` now, then every `heartbeatMs`, read from
   the response. `lastActivity` is the time of the last key press.
4. `POST /v1/circ/:room/read` on open and again when you scroll to the bottom.

The who's-here list comes from `GET /v1/circ/:room/users` when the drawer
opens. While the drawer is open, it is kept current by a second SSE stream on
`chat_presence/:room`, closed when the drawer closes. Staleness and idle are
re-checked every 10 s.

### 7.3 Messages

- **Sending:** `cs/ratelimit` is checked first (cIRC 15/min, 150/h, 1000/day),
  then `POST /v1/circ/:room`. The text shows dimmed as *sending…* until the
  stream echoes the message back. On error the text returns to the input line
  with a banner.
- **Stream events:** `put` at `/` replaces the window; `put` or `patch` at
  `/<id>` upserts. `null` data removes the message, and a patch with
  `deleted: true` turns it into a tombstone.
- **Older history:** `GET /v1/circ/:room?before=<oldest ts>`. Insertion is
  ordered by timestamp. Above 1000 messages, the oldest are dropped.

### 7.4 Leaving, screen off, background watcher

- Leaving the room or turning the screen off closes the SSE streams and sends
  `DELETE /v1/circ/:room/presence`. Waking the screen in a room re-runs §7.2
  with `startAfter` set to the newest stored timestamp.
- **Watcher**, every 60 s while signed in, screen on or off:
  - `GET /v1/circ` compares each room's `lastMessageAt` with its read marker
    and marks "new" in the room list.
  - `GET /v1/notifications?read=false&type=chat_mention` raises a buzzer and
    banner for each new mention. Mentions are marked read when that room is
    opened and scrolled to the bottom.

  That is 2 reads per minute, well inside the 30/min limits.

## 8. Web config

Served by `esp_http_server` on the network task: at `http://192.168.4.1` in
setup mode, and at `http://rhino-cyberspace.local` or the device IP on the LAN.
It is plain HTML, CSS and JS, inline, with no external requests.

- **Wi-Fi:** scan, add, remove saved networks.
- **Cyberspace account:** shows the signed-in email. A login form takes email
  and password; the device performs the login and discards the password. There
  are also Sign out and Resend verification.
- **Display:** text size, buzzer on/off, time zone (a POSIX TZ string, picked from a list or typed).
- **Security:** optional page password (HTTP Basic, user `admin`), and an
  optional setup-AP password (WPA2, 8–63 characters; clearing it makes the AP
  open again).
- **Status:** firmware version, IP, signal, battery, connection state.
- **Factory reset:** after a confirmation, erases every saved setting (Wi-Fi
  networks, account token, preferences, page and setup-AP passwords) and
  restarts. With no Wi-Fi saved the device comes up in setup mode.

Security notes, also shown on the page: the page is plain HTTP on your LAN, so
set a page password on shared networks. The setup AP is open unless a setup-AP
password is set, so anyone in radio range can join it while it runs. It runs
only with no saved Wi-Fi (first boot or after a factory reset), and anything entered on
it (Wi-Fi password, Cyberspace password) crosses that link unencrypted. Setting
a setup-AP password closes that gap for later setups. The refresh token can't
be read back through the page.

## 9. Errors and failures

| Situation | Behaviour |
|---|---|
| Wi-Fi lost | Status shows ✕. Reconnect with backoff (1 s → 60 s). Sending is disabled, not queued |
| SSE dropped or token expiry | Reopen with backoff, resuming at `startAfter`. A banner appears after 30 s down |
| Refresh rejected | Sign out, clear token, explain where to log in |
| `EMAIL_NOT_VERIFIED` | Explain, and offer to resend |
| `429` or local limit | Banner "Slow down, try in Ns". Text stays in the input line |
| `400` / `403` on send | Server message in a banner. Text stays |
| `403` room | Message, then back to the room list |
| Bad or oversized payload | Skip, log `[tag]`, continue |
| Low memory | Trim oldest messages first. SSE buffers are reserved at start |
| Time | SNTP on connect. Time zone from settings |

Serial logs are tagged (`[auth] [circ] [sse] [wifi] [web] [ui]`). Tokens and
passwords are never logged.

## 10. Testing

1. **Host unit tests** (`pio test -e native`): response parsing from fixtures
   written from the API docs and `/types.d.ts`, the SSE line parser and
   put/patch merge, `store/chat` ordering, tombstones and the cap, wrapping with
   hanging indent, the nick-colour hash (against known website values), the
   rate limiter, refresh scheduling, keymap translation, and art decoding.
2. **Fake-API firmware** (`m9_fake`): the network task is replaced by a scripted
   source that replays fixture rooms, messages, presence and errors. It
   exercises every screen, key, scroll, drawer and banner on real hardware with
   no account.
3. **Live checklist** (run by the human, written into `docs/TESTING.md`): web
   login; join a room; send a plain message, `/me`, and a bad `/dice`; scroll
   back past the first page; open the drawer and see yourself; mention yourself
   from the website and hear the buzzer with the screen off; delete your own
   message and see the tombstone on the website; Wi-Fi off and on; more than
   70 minutes of uptime across a token refresh.
4. **CI:** GitHub Actions builds `m9` and `m9_fake` and runs the `native`
   tests on every push, once the repository has a remote.

## 11. Repository layout

```
platformio.ini
src/            main.cpp, app/ (tasks, queues), hal/, input/, ui/, screens/,
                store/, chat/, cs/, net/, web/
include/        board selection
test/           native unit tests + fixtures/
docs/           USE.md, BUILD.md, TESTING.md, superpowers/specs/
scripts/        build-upload-monitor.sh (adapted from rhino-moshi)
```

## 12. Future considerations (not built now)

- **More boards:** each adds `hal/<board>/`, an `input/` keymap and a
  PlatformIO env. The UI sizes itself from the panel.
- **More Cyberspace features:** each is a new `cs/` client, `store/` model and
  `screens/` set behind the same queues. C-Mail reuses `net/sse`.
- **LoRa cIRC gateway (long term):** a separate firmware for PSRAM devices that
  bridges cIRC to LoRa for devices running this client. The only provision now
  is `chat/source`, so screens don't depend on the API transport. Before any
  work starts, it needs explicit permission from the Cyberspace operator,
  because the current API terms forbid bots and redistribution. Licence-free
  bands, not amateur bands, apply if traffic is encrypted.
