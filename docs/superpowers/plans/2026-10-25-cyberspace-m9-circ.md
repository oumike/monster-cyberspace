# Rhino Cyberspace M1 (cIRC + web config on ThinkNode M9) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. Non-Claude agents (Copilot etc.): work the tasks in order, tick the boxes, and commit after each task.

**Goal:** Standalone ESP32-S3 firmware for the Elecrow ThinkNode M9 that does Cyberspace cIRC chat, with a device-served web config page for Wi-Fi and login.

**Architecture:** Portable, Arduino-free logic (parsers, chat store, layout, widgets, keymap, rate limits) lives in `lib/core/` and is unit-tested on the host with `pio test -e native`. Firmware glue in `src/` (HAL, HTTPS, SSE, Wi-Fi, web server, tasks, screens) is built for the `m9` and `m9_fake` environments. A UI task and a network worker task talk through FreeRTOS queues. Up to two SSE reader tasks feed a mutex-guarded chat store.

**Tech Stack:** PlatformIO, pioarduino platform 55.03.312 (Arduino core 3.3 / ESP-IDF 5.5), C++17, ArduinoJson 7, ricmoo QRCode, ESP-IDF `esp_http_client` / `esp_http_server` / `mdns` / `esp_crt_bundle`, Unity (native tests), Terminus fonts (OFL) converted to 1-bpp cell fonts.

**Spec:** `docs/superpowers/specs/2026-10-25-cyberspace-m9-circ-design.md`. Read it first. Hardware references: `../camillia/camillia-mt/src/hal/hw_m9.h` and `../rhino-moshi/firmware/{include/boards/m9.h,src/panel_m9.cpp,src/buttons.cpp}` (sibling projects in the user's `Projects` folder; if absent, the pin and key values copied into this plan are sufficient).

## Global Constraints

- API base `https://api.cyberspace.online`. All requests except auth carry `Authorization: Bearer <idToken>`. Responses are `{ "data": … }`, `{ "data": [...], "cursor": … }` or `{ "error": { "code", "message" } }`.
- **Never call the live API from automated tests, scripts or an agent session.** The terms ban bots, scraping, and LLM-driven agents. Tests use fixtures; live testing is done by a human (Task 22's checklist).
- Every write originates from a key press. No automatic retries of sends.
- Client-side write limits: cIRC message 15/min, 150/hour, 1000/day. Presence heartbeat or leave 15/min per room. Delete message 5/min, 30/day. Flag 5/min, 20/hour, 50/day. Mark room read 60/min.
- Message content max 2,048 characters (cIRC). Flag reason max 500. `/art` canvas 80×25.
- Never log `idToken`, `refreshToken`, `rtdbToken` or passwords. Never store the password.
- Setup AP SSID `Rhino-Cyberspace-<BOARD_NAME>-Setup` (`Rhino-Cyberspace-M9-Setup`). **Open unless** a setup-AP password (8–63 chars) is configured. **No captive portal, no DNS hijack.** Page at `http://192.168.4.1`, and on the LAN at `http://rhino-cyberspace.local`.
- M9 console is the external UART bridge: `-DARDUINO_USB_CDC_ON_BOOT=0`, 115200 baud.
- Keyboard I2C pins 20/21 are the S3's USB pads. Release the USB-Serial-JTAG pad claim before `Wire.begin(20, 21)`. Never drive GPIO46. Never touch GPIO35–37 (octal PSRAM).
- Panel: ST7789, native 240×320, landscape rotation MADCTL `0xA0`, inverted (`0x21`), SPI SCLK 40 / MOSI 47 / MISO 38, CS 16, DC 15, RST 14, SPI 40 MHz. Backlight GPIO17 **active LOW**, LEDC channel 7. VEXT GPIO18 **active LOW** (powers panel and keyboard). SD CS 48 and LoRa CS 39 must be driven HIGH before using SPI.
- Keyboard: STC8H at 0x6C (also try 0x6D). HW version reg 0x00, FW reg 0xFE. Key reg 0x05 (production) or 0x01 (early: hw 0x03, fw 0x10). INT GPIO12 active HIGH. 0 means no key; there are no release events.
- Buzzer GPIO9 (passive; `tone()` must not use LEDC channel 7). Battery ADC GPIO13, divider 2.0.
- Times: SNTP on connect. Time zone is a POSIX TZ string from settings (default `UTC0`), applied with `setenv("TZ")` + `tzset()`.
- Allocate chat data in PSRAM: call `heap_caps_malloc_extmem_enable(64)` first thing in `setup()`.

## Review Focus

1. **Non-ASCII text** (emoji, accents, CJK in nicks and messages): wrap and input editing must never split a UTF-8 sequence; missing glyphs draw a fallback box, one cell wide. Tests in Tasks 2, 4 and 11.
2. **SSE frames split across reads, CRLF endings, multi-line `data:`**: the parser must handle chunking at every byte boundary. Test in Task 6.
3. **History and stream overlap or out-of-order delivery**: the REST page and the SSE window repeat messages, and late messages arrive older than the newest. Upsert by id, keep timestamp order, no duplicates. Tests in Tasks 5 and 7.
4. **Clock not yet synced** (SNTP pending) when scheduling token refresh or rendering times: refresh falls back to receipt + 55 min, and times render from server timestamps. Tests in Tasks 9 and 11.
5. **Pathological message sizes**: a 2,048-char message with no spaces, `/art` wider than the screen, and empty content with only an attachment. Hard-split, clip, and no double-printing of URLs. Tests in Tasks 4 and 11.

---

## File map

```
platformio.ini                         envs: m9, m9_fake, native
lib/core/library.json                  portable library (no Arduino)
lib/core/src/util/utf8.{h,cpp}         UTF-8 decode/encode
lib/core/src/util/base64.{h,cpp}
lib/core/src/ui/nick_color.{h,cpp}     FNV-1a hue, HSL→RGB565, theme
lib/core/src/ui/theme.{h,cpp}
lib/core/src/ui/wrap.{h,cpp}           word wrap with hanging indent
lib/core/src/ui/grid.{h,cpp}           cell buffer + dirty rows
lib/core/src/ui/timefmt.{h,cpp}        HH:MM, day label
lib/core/src/ui/room_layout.{h,cpp}    messages → DisplayLines (Comfortable/Compact)
lib/core/src/ui/widgets/*.{h,cpp}      input_line, log_view, list_view, drawer, status_bar, banner, palette
lib/core/src/ui/qr_cells.{h,cpp}       QR → half-block cells
lib/core/src/chat/message.h            CircMessage
lib/core/src/chat/chat_store.{h,cpp}
lib/core/src/chat/presence.{h,cpp}
lib/core/src/chat/mention.{h,cpp}
lib/core/src/cs/message_json.{h,cpp}   JSON ↔ CircMessage (create + merge)
lib/core/src/cs/api_parse.{h,cpp}      REST response parsers
lib/core/src/cs/rtdb.{h,cpp}           stream URL builder + event apply
lib/core/src/cs/rate_limiter.{h,cpp}   + limits factories
lib/core/src/cs/token.{h,cpp}          JWT exp + refresh schedule
lib/core/src/net/sse_parser.{h,cpp}
lib/core/src/input/keymap_m9.{h,cpp}   raw code → KeyEvent
lib/core/src/web/web_util.{h,cpp}      AP SSID, password rules, Basic auth
test/test_<module>/test_main.cpp       one Unity suite per module
test/fixtures/*.h                      raw-string API fixtures
scripts/bdf2cells.py                   Terminus BDF → cell font header
src/ui/fonts/cell6x12.h, cell8x16.h    generated (committed)
src/hal/board.h                        board-neutral HAL interface
src/hal/m9/{board_m9.cpp,panel_st7789.{h,cpp},keyboard_stc8h.{h,cpp}}
src/ui/renderer.{h,cpp}                Grid → framebuffer → panel (dirty rows)
src/store/settings.{h,cpp}             NVS
src/store/locked_chat.{h,cpp}          mutex wrapper over ChatStore
src/net/wifi_mgr.{h,cpp}  src/net/https_client.{h,cpp}  src/net/sse_client.{h,cpp}
src/cs/cs_api.{h,cpp}                  auth + circ + notify over HttpsClient
src/app/messages.h                     Command / Event structs
src/app/queues.{h,cpp}  src/app/net_worker.{h,cpp}  src/app/ui_task.{h,cpp}
src/chat/chat_source.h  src/chat/net_chat_source.{h,cpp}  src/chat/fake_chat_source.{h,cpp}
src/screens/{router,splash,setup,home,room_list,room,coming_soon,login_form}.{h,cpp}
src/web/{web_server.{h,cpp},page.h}
src/main.cpp
docs/{BUILD.md,USE.md,TESTING.md}  .github/workflows/build.yml  AGENTS.md
```

---

### Task 1: Project scaffold, build environments, spec amendments

**Files:**
- Create: `platformio.ini`, `lib/core/library.json`, `lib/core/src/core_version.h`, `test/test_smoke/test_main.cpp`, `src/main.cpp`, `scripts/build-upload-monitor.sh`, `docs/BUILD.md`
- Modify: `docs/superpowers/specs/2026-10-25-cyberspace-m9-circ-design.md` §3, §4

**Interfaces:**
- Produces: `#define RC_VERSION "0.1.0"` in `core_version.h`. Envs `m9`, `m9_fake` (adds `-DRC_FAKE_API=1`) and `native`. Every later task adds sources under these paths.

- [x] **Step 1: Write `platformio.ini`**
  - `[env]`: `platform = https://github.com/pioarduino/platform-espressif32/releases/download/55.03.312/platform-espressif32.zip`, `framework = arduino`, `monitor_speed = 115200`, `upload_speed = 921600`, `lib_deps = bblanchon/ArduinoJson@^7.0.0, ricmoo/QRCode@^0.0.1`.
  - `[env:m9]`: `board = esp32-s3-devkitc-1`, `board_build.arduino.memory_type = qio_opi`, `board_build.flash_mode = qio`, `board_upload.flash_size = 16MB`, `board_upload.maximum_size = 16777216`, `board_build.partitions = default_16MB.csv`, `build_flags = -DBOARD_M9 -DBOARD_HAS_PSRAM -DARDUINO_USB_MODE=1 -DARDUINO_USB_CDC_ON_BOOT=0 -std=gnu++17`, `build_unflags = -std=gnu++11`.
  - `[env:m9_fake]`: `extends = env:m9`, `build_flags = ${env:m9.build_flags} -DRC_FAKE_API=1`.
  - `[env:native]`: `platform = native`, `framework =` (empty), `test_framework = unity`, `build_flags = -std=gnu++17 -DRC_NATIVE=1`, `lib_compat_mode = off`, `lib_deps = bblanchon/ArduinoJson@^7.0.0, ricmoo/QRCode@^0.0.1`.
- [x] **Step 2: Write `lib/core/library.json`** as `{"name":"core","version":"0.1.0","build":{"flags":"-std=gnu++17"}}`, plus `core_version.h`.
- [x] **Step 3: Write the smoke test** `test/test_smoke/test_main.cpp`, asserting `TEST_ASSERT_EQUAL_STRING("0.1.0", RC_VERSION)` with a Unity `main()`.
- [x] **Step 4: Run** `pio test -e native`. Expected: `test_smoke … PASSED`.
- [x] **Step 5: Write `src/main.cpp`**: `setup()` calls `heap_caps_malloc_extmem_enable(64)`, `Serial.begin(115200)`, and prints `[boot] rhino-cyberspace <RC_VERSION> board=M9`. `loop()` does `delay(1000)`.
- [x] **Step 6: Run** `pio run -e m9 && pio run -e m9_fake`. Expected: both `SUCCESS`.
- [x] **Step 7:** Copy `../rhino-moshi/scripts/build-upload-monitor.sh` (or write an equivalent: `pio run -e "${1:-m9}" -t upload && pio device monitor -b 115200`). Write `docs/BUILD.md` covering prerequisites (PlatformIO Core ≥ 6.1), the three envs, the upload command, and the serial monitor.
- [x] **Step 8: Amend the spec.**
  - §3: replace the LovyanGFX line with "Display: a direct ST7789 driver with a full-frame RGB565 canvas in PSRAM, pushing dirty row bands (ported from rhino-moshi `panel_m9.cpp`)".
  - §4: add that each open SSE stream (at most two: room messages, room presence) runs in its own reader task on core 0, and that the chat store is shared under a mutex.
  - §6/§8: the time zone is a POSIX TZ string.
- [x] **Step 9: Commit** `chore: scaffold PlatformIO project (m9, m9_fake, native)`.

### Task 2: UTF-8 and base64 utilities

**Files:** Create `lib/core/src/util/utf8.{h,cpp}`, `lib/core/src/util/base64.{h,cpp}`, `test/test_util/test_main.cpp`

**Interfaces (namespace `rc`):**
- `std::u32string utf8ToU32(std::string_view s);` Invalid bytes → U+FFFD, consuming 1 byte.
- `std::string u32ToUtf8(std::u32string_view s);`
- `size_t utf8Length(std::string_view s);` Counts code points.
- `bool base64Decode(std::string_view in, std::string& out);` Standard alphabet; ignores `\n`/`\r`; false on any other invalid char.

- [x] **Step 1: Write failing tests:**
  - `test_utf8_roundtrip`: `"héllo 🎲"` → 7 code points → back equal.
  - `test_utf8_invalid`: `"a\xffb"` → `{U'a', 0xFFFD, U'b'}`.
  - `test_utf8_truncated`: `"\xe2\x94"` → one or two U+FFFD with no crash.
  - `test_base64_art`: `"IC9cXy9cCiggby5vICkKID4gXiA8"` → `" /\\_/\\\n( o.o )\n > ^ <"`.
  - `test_base64_invalid`: `"@@@"` → false.
- [x] **Step 2: Run** `pio test -e native -f test_util`. Expected: FAIL (link errors).
- [x] **Step 3: Implement.**
- [x] **Step 4: Run** the same. Expected: PASS.
- [x] **Step 5: Commit** `feat(core): utf8 and base64 utilities`.

### Task 3: Nick colours and theme

**Files:** Create `lib/core/src/ui/nick_color.{h,cpp}`, `lib/core/src/ui/theme.{h,cpp}`, `test/test_nick_color/test_main.cpp`

**Interfaces:**
- `uint32_t fnv1a32Lower(std::string_view name);` FNV-1a 32-bit (offset 2166136261, prime 16777619) over the ASCII-lower-cased bytes.
- `uint16_t nickHue(std::string_view name);` Returns `fnv1a32Lower(name) % 360`.
- `uint16_t hslToRgb565(float hDeg, float s, float l);`
- `uint16_t nickColor(std::string_view name, bool isSelf);` Self → gold `0xFE8C` (#FFD166); others → `hslToRgb565(nickHue, 0.70f, 0.68f)`.
- `struct Theme { uint16_t bg, fg, dim, barBg, barFg, hintBg, hintFg, time, sys, mentionBg, inputBg, cursorBg, cursorFg, selectBg, selectFg, drawerBg, drawerEdge, error; };` `const Theme& defaultTheme();`. Values follow the mockups: bg black, fg #B8C0CC, dim #555555, bar #1B2A1B on #7CFC7C, time #5B9BFF, sys #6C7A89, mention #22223A, select #7CFC7C on black, error #FF5555.

- [x] **Step 1: Write failing tests:**
  - `test_fnv_values`: `fnv1a32Lower("tardibyte")==1666852401u`, `("genghis_khan")==2518148919u`, `("oumike")==727871219u`, `("A")==3826002220u`, `("")==2166136261u`.
  - `test_hue`: tardibyte 201, genghis_khan 39, oumike 179, A 340.
  - `test_hsl`: `hslToRgb565(0,1,0.5f)==0xF800`, `(120,1,0.5f)==0x07E0`, `(240,1,0.5f)==0x001F`, `(0,0,1)==0xFFFF`.
  - `test_self_gold`: `nickColor("x",true)==0xFE8C`.
- [x] **Step 2: Run** `pio test -e native -f test_nick_color`. Expected: FAIL.
- [x] **Step 3: Implement.**
- [x] **Step 4: Run.** Expected: PASS.
- [x] **Step 5: Commit** `feat(core): website nick-colour hash and default theme`.

### Task 4: Word wrap with hanging indent

**Files:** Create `lib/core/src/ui/wrap.{h,cpp}`, `test/test_wrap/test_main.cpp`

**Interfaces:**
- `std::vector<std::u32string> wrapText(std::u32string_view text, int firstWidth, int contWidth);` Each code point is one cell. Breaks after spaces. A word longer than the width is hard-split. `\n` forces a break. Trailing spaces at a break are dropped. Empty text → one empty line. Widths < 1 are treated as 1.

- [x] **Step 1: Write failing tests:**
  - `test_fits`: `"hi there"`, widths 20/20 → `{"hi there"}`.
  - `test_wraps_at_space`: `"anyone else on the 4.0 beta yet?"` with 20/14 → `{"anyone else on the","4.0 beta yet?"}`.
  - `test_hard_split`: 2,048 × `'a'` with 47/41 → line 0 is 47 chars, the rest ≤ 41, total chars 2,048.
  - `test_newline`: `"a\nb"` → `{"a","b"}`.
  - `test_multibyte_unsplit`: `"🎲🎲🎲"` with width 2 → `{"🎲🎲","🎲"}`.
  - `test_empty`: `""` → `{""}`.
- [x] **Step 2: Run** `pio test -e native -f test_wrap`. Expected: FAIL.
- [x] **Step 3: Implement.**
- [x] **Step 4: Run.** Expected: PASS.
- [x] **Step 5: Commit** `feat(core): word wrap with hanging indent`.

### Task 5: Message model, chat store, mentions

**Files:** Create `lib/core/src/chat/message.h`, `lib/core/src/chat/chat_store.{h,cpp}`, `lib/core/src/chat/mention.{h,cpp}`, `test/test_chat_store/test_main.cpp`

**Interfaces:**
- `struct CircMessage { std::string id, userId, username, content; int64_t timestamp = 0; bool isChatAdmin=false, isAction=false, isDice=false, isEightball=false, isFortune=false, deleted=false; std::vector<std::string> styles; std::string imageUrl, gifUrl, eightballAnswer, fortuneText; struct Audio { std::string src, origin, artist, title, genre; } audio; bool hasAudio=false; };`
- `class ChatStore { public: explicit ChatStore(size_t cap = 1000); void clear(); void replaceAll(std::vector<CircMessage> msgs); CircMessage& upsert(CircMessage msg); CircMessage* find(const std::string& id); bool remove(const std::string& id); const std::deque<CircMessage>& messages() const; size_t size() const; int64_t newestTimestamp() const; int64_t oldestTimestamp() const; uint32_t version() const; };`
  - Messages are kept ascending by `(timestamp, id)`. Upserting an existing id replaces it and re-sorts if its timestamp changed. Over the cap, the oldest are dropped. `version()` increments on every mutation. Newest and oldest are `-1` when empty. `replaceAll` sorts and dedupes.
- `bool mentionsUser(std::string_view content, std::string_view username);` Case-insensitive `@username`. The preceding char must be start or non-word, and the following char end or non-word (word = `[A-Za-z0-9_]`).

- [x] **Step 1: Write failing tests:**
  - `test_upsert_orders`: insert ts 30, 10, 20 → order 10, 20, 30.
  - `test_upsert_dedupes`: the same id twice → size 1, latest content kept.
  - `test_cap_drops_oldest`: cap 3, insert 4 → the oldest ts is gone.
  - `test_remove`: remove an existing id → true and size drops; remove an unknown id → false.
  - `test_version_bumps`: version increases after upsert and remove.
  - `test_replace_all_dedupes_and_sorts`.
  - `test_overlap_history_and_stream`: replaceAll of ids a, b, c, then upsert b (changed) and d → order a, b, c, d with b updated.
  - `test_mentions`: `mentionsUser("hey @OuMike!", "oumike")` → true. `"@oumikes"` → false. `"mail@oumike"` → false. `"@oumike"` → true.
- [x] **Step 2: Run** `pio test -e native -f test_chat_store`. Expected: FAIL.
- [x] **Step 3: Implement** with a `std::deque` and binary-search insertion.
- [x] **Step 4: Run.** Expected: PASS.
- [x] **Step 5: Commit** `feat(core): ordered chat store and mention detection`.

### Task 6: SSE parser

**Files:** Create `lib/core/src/net/sse_parser.{h,cpp}`, `test/test_sse/test_main.cpp`

**Interfaces:**
- `struct SseEvent { std::string event; std::string data; };`
- `class SseParser { public: using Handler = std::function<void(const SseEvent&)>; void feed(const char* buf, size_t len, const Handler& onEvent); void reset(); size_t maxEventBytes = 256 * 1024; bool overflowed() const; };`
  - Per the SSE spec: lines end with `\n`, `\r\n` or `\r`. `event:` sets the name. `data:` lines are joined with `\n`. A blank line dispatches. Lines starting with `:` are comments. One optional space after the colon is stripped. Above `maxEventBytes`, the event is discarded and `overflowed()` is set until the next dispatch.

- [x] **Step 1: Write failing tests:**
  - `test_single_event`: `"event: put\ndata: {\"path\":\"/\",\"data\":null}\n\n"` → one event, name `put`, data exact.
  - `test_crlf`: the same with `\r\n`.
  - `test_multiline_data`: two `data:` lines → joined with `\n`.
  - `test_every_split_point`: for every split index `i` of a two-event stream, `feed(0..i)` then `feed(i..)` yields the same two events.
  - `test_keepalive`: `"event: keep-alive\ndata: null\n\n"` → name `keep-alive`.
  - `test_comment_ignored`.
  - `test_overflow`: `maxEventBytes = 16` with 100-byte data → no event, `overflowed()` true. A following small event still parses.
- [x] **Step 2: Run** `pio test -e native -f test_sse`. Expected: FAIL.
- [x] **Step 3: Implement.**
- [x] **Step 4: Run.** Expected: PASS.
- [x] **Step 5: Commit** `feat(core): incremental SSE parser`.

### Task 7: Message JSON, RTDB URLs and stream apply, presence

**Files:** Create `lib/core/src/cs/message_json.{h,cpp}`, `lib/core/src/cs/rtdb.{h,cpp}`, `lib/core/src/chat/presence.{h,cpp}`, `test/test_rtdb/test_main.cpp`, `test/fixtures/rtdb_fixtures.h`

**Interfaces:**
- `void mergeMessageJson(CircMessage& m, JsonObjectConst o);` Sets only the keys present: `userId`, `username`, `content`, `timestamp`, `isChatAdmin`, `isAction`, `isDice`, `isEightball`, `eightballAnswer`, `isFortune`, `fortuneText`, `imageUrl`, `gifUrl`, `deleted`. `style` may be a string or an array → `styles`. `audioAttachment{src,origin,cover,artist,title,genre}` → `audio` with `hasAudio = true`. When `deleted` becomes true, clear `styles`, `imageUrl`, `gifUrl`, `audio`/`hasAudio` and the dice, 8-ball and fortune fields.
- `CircMessage messageFromJson(const std::string& id, JsonObjectConst o);` Equivalent to `CircMessage m; m.id = id; merge`.
- `struct RtdbQuery { bool orderByTimestamp = false; int64_t startAfter = -1; int limitToLast = 0; };`
- `std::string rtdbStreamUrl(const std::string& base, const std::string& path, const std::string& idToken, const RtdbQuery& q);` Builds `base + "/" + path + ".json?auth=" + token`, then `"&orderBy=%22timestamp%22"` if set, then `"&startAfter=" + n` if ≥ 0, then `"&limitToLast=" + n` if > 0.
- `enum class StreamResult { Changed, Unchanged, KeepAlive, Cancel, AuthRevoked, BadData };`
- `StreamResult applyChatEvent(ChatStore& store, const SseEvent& ev);`
  - `put` at path `/`: data is an object of id → message, each upserted fresh; null is a no-op. (Amended: the stream opens with `startAfter=<newest stored>`, so its window doesn't cover REST history, and replacing or clearing would wipe it.)
  - `put` at `/<id>`: an object upserts a fresh message; null removes it.
  - `patch` at `/`: an object of id → partial merges into existing messages, or upserts new ones.
  - `patch` at `/<id>`: a partial merge.
  - Event names `keep-alive`, `cancel` and `auth_revoked` map to the matching results.
- `struct PresenceEntry { std::string userId, username; bool online=false, isChatAdmin=false; int64_t lastSeen=0, lastActivity=-1; };` `using PresenceMap = std::map<std::string, PresenceEntry>;`
- `StreamResult applyPresenceEvent(PresenceMap& map, const SseEvent& ev);` Same path rules, keyed by userId, except a `put` at `/` replaces the map (null clears it).
- `std::vector<PresenceEntry> visiblePresence(const PresenceMap& map, int64_t nowMs, int64_t staleAfterMs);` Online and `nowMs - lastSeen <= staleAfterMs`, sorted case-insensitively by username.
- `bool isIdle(const PresenceEntry& e, int64_t nowMs, int64_t idleAfterMs);` False when `lastActivity == -1`.

- [x] **Step 1: Write fixtures**, using the docs' shapes: a window put of two messages, a single put, a delete patch `{"content":"[DELETED]","deleted":true}`, a null put, an art message (`style:"art"`, content `"IC9cXy9cCiggby5vICkKID4gXiA8"`), and a presence window with one stale, one idle and one active entry.
- [x] **Step 2: Write failing tests:**
  - `test_url_messages`: `rtdbStreamUrl("https://x.firebasedatabase.app","chat_messages/general","T",{true,123,1000})` equals `"https://x.firebasedatabase.app/chat_messages/general.json?auth=T&orderBy=%22timestamp%22&startAfter=123&limitToLast=1000"`.
  - `test_url_presence`: `{false,-1,0}` → `"…/chat_presence/general.json?auth=T"`.
  - `test_window_put`, `test_child_put`, `test_null_put_removes`.
  - `test_delete_patch_tombstones`: content `[DELETED]`, `deleted` true, `gifUrl` cleared.
  - `test_style_array_and_string`.
  - `test_keepalive_cancel_revoked`.
  - `test_bad_json` → `BadData` with the store untouched.
  - `test_presence_visible_and_idle`: now = 1,000,000; staleAfter 180,000; idleAfter 600,000 → the stale entry is hidden, the idle one reports true, and the order is by name.
- [x] **Step 3: Run** `pio test -e native -f test_rtdb`. Expected: FAIL.
- [x] **Step 4: Implement** with ArduinoJson `JsonDocument` + `deserializeJson`.
- [x] **Step 5: Run.** Expected: PASS.
- [x] **Step 6: Commit** `feat(core): RTDB stream URLs, chat and presence event apply`.

### Task 8: REST response parsers

**Files:** Create `lib/core/src/cs/api_parse.{h,cpp}`, `test/test_api_parse/test_main.cpp`, `test/fixtures/api_fixtures.h`

**Interfaces:**
- `struct Tokens { std::string idToken, refreshToken, rtdbUrl; };`
- `struct ApiError { int http = 0; std::string code, message; bool ok() const { return http >= 200 && http < 300; } };`
- `ApiError parseError(int http, std::string_view body);` Fills `code` and `message` from the `error` envelope. On non-JSON, code is `"HTTP_<n>"` and message is `"Server error"`.
- `bool parseTokens(std::string_view body, Tokens& out);` Login and refresh. Refresh may omit `refreshToken`; keep the existing one.
- `struct RoomInfo { std::string id, slug, name; int64_t lastMessageAt = 0; int sortOrder = 0, onlineCount = 0; };` `bool parseRooms(std::string_view body, std::vector<RoomInfo>& out);`
- `bool parseHistory(std::string_view body, std::vector<CircMessage>& out, int64_t& cursor);` Messages carry `id` inside each element. `cursor` is a number, or `-1` when null or absent.
- `bool parseUsers(std::string_view body, std::vector<PresenceEntry>& out);` Users are online by definition.
- `struct PresenceCadence { int64_t heartbeatMs = 30000, staleAfterMs = 180000, idleAfterMs = 600000; };` `bool parsePresenceAck(std::string_view body, PresenceCadence& out);`
- `bool parseSendResult(std::string_view body, std::string& messageId, std::string& reply);` `reply` comes from `data.reply` (`/help`, `/mute`).
- `struct Mention { std::string id, roomSlug, roomName, actorUsername, messageContent; };` `bool parseMentions(std::string_view body, std::vector<Mention>& out);` Reads `metadata.roomSlug`, `metadata.roomName`, `metadata.messageContent`.
- `bool parseUnreadCount(std::string_view body, int& count, bool& exact);`

- [x] **Step 1: Write fixtures** copied from the spec and API docs shapes: a login response, a refresh without `refreshToken`, a rooms list of three rooms, a history page with cursor `1719700000000`, a history page with `"cursor":null`, users with an idle `lastActivity`, a presence ack with `heartbeatMs 30000`, `staleAfterMs 180000` and `idleAfterMs 600000`, a send result, a `/help` reply, a `chat_mention` notification list, an unread count `{count:100,exact:false}`, and errors `RATE_LIMITED 429`, `EMAIL_NOT_VERIFIED 403` and an HTML 502 body.
- [x] **Step 2: Write failing tests**, one per parser, asserting every field above, plus:
  - `test_error_non_json`: 502 HTML → code `"HTTP_502"`.
  - `test_history_null_cursor` → `-1`.
  - `test_message_empty_content_with_image` → content `""` and `imageUrl` set.
- [x] **Step 3: Run** `pio test -e native -f test_api_parse`. Expected: FAIL.
- [x] **Step 4: Implement**, reusing `messageFromJson`.
- [x] **Step 5: Run.** Expected: PASS.
- [x] **Step 6: Commit** `feat(core): REST response parsers with fixtures`.

### Task 9: Rate limiter and token refresh schedule

**Files:** Create `lib/core/src/cs/rate_limiter.{h,cpp}`, `lib/core/src/cs/token.{h,cpp}`, `test/test_limits_token/test_main.cpp`

**Interfaces:**
- `class RateLimiter { public: struct Window { uint32_t limit; int64_t periodMs; }; explicit RateLimiter(std::vector<Window> w); bool tryAcquire(int64_t nowMs); int64_t retryAfterMs(int64_t nowMs) const; };` A sliding log of acquire times (keep at most the largest limit). `tryAcquire` records only on success.
- Factories, values from Global Constraints:
  - `RateLimiter makeCircSendLimiter();` 15/60,000, 150/3,600,000, 1000/86,400,000.
  - `RateLimiter makePresenceLimiter();` 15/60,000.
  - `RateLimiter makeDeleteLimiter();` 5/60,000, 30/86,400,000.
  - `RateLimiter makeFlagLimiter();` 5/60,000, 20/3,600,000, 50/86,400,000.
  - `RateLimiter makeMarkReadLimiter();` 60/60,000.
- `int64_t jwtExpSeconds(const std::string& jwt);` Base64url-decodes the payload and reads `exp`. Returns `-1` on any failure.
- `int64_t refreshDueMonoMs(int64_t receivedMonoMs, int64_t nowEpochSec, int64_t expSec);`
  - If `nowEpochSec >= 1700000000` and `expSec > 0`: `receivedMonoMs + max(60, expSec - nowEpochSec - 300) * 1000`.
  - Otherwise: `receivedMonoMs + 55 * 60 * 1000`.

- [x] **Step 1: Write failing tests:**
  - `test_circ_minute`: 15 acquires at t=0..14 s succeed, the 16th at 15 s fails, and `retryAfterMs(15000) == 45000`. At t = 60,001 it succeeds.
  - `test_circ_hourly`: 150 acquires spaced 5 s apart all pass, and the 151st at the next 5 s step fails.
  - `test_jwt_exp`: `"eyJhbGciOiJSUzI1NiJ9.eyJleHAiOiAxNzkyOTYwMDAwLCAidXNlcl9pZCI6ICJ1MSJ9.c2ln"` → `1792960000`. Garbage → `-1`.
  - `test_refresh_synced`: received 1000, now 1792956400, exp 1792960000 → `1000 + 3300*1000`.
  - `test_refresh_unsynced`: now 0 → `1000 + 3300000`.
  - `test_refresh_floor`: exp − now = 100 → `1000 + 60000`.
- [x] **Step 2: Run** `pio test -e native -f test_limits_token`. Expected: FAIL.
- [x] **Step 3: Implement.**
- [x] **Step 4: Run.** Expected: PASS.
- [x] **Step 5: Commit** `feat(core): client-side rate limits and token refresh schedule`.

### Task 10: M9 keymap and web utilities

**Files:** Create `lib/core/src/input/keymap_m9.{h,cpp}`, `lib/core/src/web/web_util.{h,cpp}`, `test/test_keymap_web/test_main.cpp`

**Interfaces:**
- `enum class Action { None, Char, Backspace, Select, Back, Up, Down, Left, Right, OpenCirc, OpenHome, OpenCmail, OpenFeed, OpenMap, Palette, ClearLine, ScreenOff };`
- `struct KeyEvent { Action action = Action::None; char32_t ch = 0; };`
- `KeyEvent mapM9Key(uint8_t raw, bool earlyController);` Mapping:

  | Raw code | Action |
  |---|---|
  | `0x08`, `0x7F` | Backspace |
  | `0x0D`, `0x0A` | Select |
  | `0x81` | OpenCirc |
  | `0x82` | OpenHome |
  | `0x83` | OpenCmail |
  | `0x84` | OpenFeed |
  | `0x85` | OpenMap |
  | `0x86` | Back iff early, else None |
  | `0x87` | Back iff not early, else None |
  | `0x88`, `0x90` | Palette |
  | `0x89` | ClearLine |
  | `0xA3` | ScreenOff |
  | `0xB4` / `0xB5` / `0xB6` / `0xB7` | Left / Up / Down / Right |
  | `0x20`–`0x7E` | Char |
  | anything else | None |

- `bool wakesScreen(const KeyEvent& e);` True only for `ScreenOff` (centre hold).
- `bool peeksScreen(const KeyEvent& e);` True only for `Select` (centre tap): shows the screensaver while dark (spec §5).
- `std::string setupApSsid(std::string_view boardName);` Returns `"Rhino-Cyberspace-" + boardName + "-Setup"`.
- `bool validApPassword(std::string_view pw);` Empty → true (open AP). Otherwise 8–63 printable ASCII.
- `bool checkBasicAuth(std::string_view authorizationHeader, std::string_view password);` Empty password → true. Otherwise requires `Basic base64("admin:" + password)`.

- [x] **Step 1: Write failing tests:**
  - Every row of the table, for both `early = true` and `false` where it matters.
  - `wakesScreen` is true only for ScreenOff (centre hold).
  - `setupApSsid("M9") == "Rhino-Cyberspace-M9-Setup"`.
  - Passwords: `""` true, `"short"` false, 8 chars true, 64 chars false.
  - `checkBasicAuth("Basic YWRtaW46cGFzcw==", "pass")` true, the wrong password false, `""` password → true.
- [x] **Step 2: Run** `pio test -e native -f test_keymap_web`. Expected: FAIL.
- [x] **Step 3: Implement.**
- [x] **Step 4: Run.** Expected: PASS.
- [x] **Step 5: Commit** `feat(core): M9 keymap and web config helpers`.

### Task 11: Grid, time formatting, room layout

**Files:** Create `lib/core/src/ui/grid.{h,cpp}`, `lib/core/src/ui/timefmt.{h,cpp}`, `lib/core/src/ui/room_layout.{h,cpp}`, `test/test_layout/test_main.cpp`

**Interfaces:**
- `struct Cell { char32_t cp = U' '; uint16_t fg = 0, bg = 0; bool operator==(const Cell&) const; };`
- `class Grid { public: Grid(int cols, int rows); int cols() const; int rows() const; void fill(uint16_t bg); void fillRow(int row, uint16_t bg); void put(int col, int row, char32_t cp, uint16_t fg, uint16_t bg); int putText(int col, int row, std::u32string_view s, uint16_t fg, uint16_t bg, int maxCols); const Cell& at(int col, int row) const; bool rowDirty(int row) const; void clearDirty(); void resize(int cols, int rows); };` A put marks the row dirty only when the cell changes. Out-of-range puts are ignored.
- `enum class TextSize { Comfortable, Compact };` `struct CellSize { int w, h; };` `CellSize cellSize(TextSize s);` → 8×16 or 6×12. `void gridDims(int panelW, int panelH, TextSize s, int& cols, int& rows);` → 320×240 gives 40×15 and 53×20.
- `std::string formatHHMM(int64_t epochMs);` `std::string formatDay(int64_t epochMs);` (`"Sat 25 Oct"`). Both use `localtime_r` on the process TZ.
- `struct Span { std::u32string text; uint16_t fg; };` `struct DisplayLine { std::vector<Span> spans; std::string messageId; bool mentionTint = false; bool divider = false; };`
- `struct LayoutOptions { TextSize size; int cols; std::string selfUsername; const std::set<std::string>* muted = nullptr; const std::set<std::string>* revealed = nullptr; const Theme* theme = nullptr; };`
- `std::vector<DisplayLine> layoutRoom(const std::deque<CircMessage>& msgs, const LayoutOptions& o);`

**Layout rules (spec §6):**
- Muted usernames are skipped.
- **Compact:**
  - A date divider (`── Sat 25 Oct ──`, dim, full width) before the first message and whenever the day changes.
  - First line: `HH:MM ` in the time colour, then `nick` in the nick colour, then `: ` and the body.
  - Continuation lines are indented to the body start, which is col 6 + nick length + 2, capped at col 20. Past that cap, continuations indent to col 6.
- **Comfortable:**
  - A dim `── HH:MM ──…` divider when the minute differs from the previous message.
  - The nick is padded to 10 columns plus a space, and the body starts at col 11. Continuations are indented 11. Nicks longer than 10 are cut with `…`.
- **Body by kind:**
  - Action: `* nick body` in `sys`, with no `: `.
  - Dice: the body as given.
  - 8-ball: `🎱 <content> → <eightballAnswer>`.
  - Fortune: `🥠 <fortuneText>`.
  - Deleted: `[deleted]` in dim.
  - Spoiler, if not in `revealed`: `▒▒ spoiler · ● to reveal ▒▒`.
  - Art: the decoded lines verbatim, each clipped to the available width, with no wrapping.
  - Attachments append `[img] url`, `[gif] url` or `[song] artist – title`. Content equal to `imageUrl`/`gifUrl` is not printed.
- `mentionTint` is true on every line of a message that mentions `selfUsername`.
- Every line of a message carries that message's `messageId`. Divider lines carry `divider = true` and an empty id.

- [x] **Step 1: Write failing tests** (set `TZ=UTC0` in `setUp`):
  - `test_grid_dims`: 40×15 and 53×20.
  - `test_grid_dirty`: the same put twice marks dirty once; after `clearDirty`, putting the same cell leaves it clean.
  - `test_time`: `formatHHMM(1761400920000) == "14:02"` and `formatDay(...) == "Sat 25 Oct"`.
  - `test_compact_basic`: given ts 1761400920000, username `tardibyte` and content `"anyone else on the 4.0 beta yet? the grid build tools are finally in"`, the line spans flatten to `"14:02 tardibyte: anyone else on the 4.0 beta yet? the"` at 53 cols, and the continuation starts with 17 spaces.
  - `test_compact_day_divider_first`.
  - `test_comfortable_minute_divider`: two messages in the same minute → one divider; a new minute → a second divider.
  - `test_action_line`: `* rain slaps tardibyte…`.
  - `test_deleted`, `test_spoiler_hidden_then_revealed`.
  - `test_art_clipped`: decoded art lines, each ≤ cols.
  - `test_image_only_no_double_url`: content == `imageUrl` → a single `[img] url` line.
  - `test_mention_tint`.
  - `test_muted_skipped`.
  - `test_long_no_space_message`: 2,048 chars → a line count equal to `ceil` of the first and continuation widths, with no line longer than cols.
  - `test_emoji_body`: an emoji counts as one cell.
- [x] **Step 2: Run** `pio test -e native -f test_layout`. Expected: FAIL.
- [x] **Step 3: Implement** using `wrapText`, `nickColor` and `base64Decode`.
- [x] **Step 4: Run.** Expected: PASS.
- [x] **Step 5: Commit** `feat(core): cell grid and cIRC room layout (comfortable/compact)`.

### Task 12: Widgets

**Files:** Create `lib/core/src/ui/widgets/{input_line,log_view,list_view,drawer,status_bar,banner,palette}.{h,cpp}`, `lib/core/src/ui/qr_cells.{h,cpp}`, `test/test_widgets/test_main.cpp`

**Interfaces (all render into `Grid&` with a `const Theme&`):**
- `class InputLine { void insert(char32_t c); void backspace(); bool left(); bool right(); bool atEnd() const; void clear(); void setText(std::u32string_view s); std::u32string text() const; std::string utf8() const; size_t cursor() const; void setDisabled(bool); void render(Grid&, int row, const Theme&, std::u32string_view prompt = U"> ") const; static constexpr size_t kMaxBytes = 2048; };`
  - `insert` refuses past `kMaxBytes` of UTF-8.
  - `right()` returns false when already at the end. The room screen uses that to toggle the drawer.
  - It scrolls horizontally to keep the cursor visible.
  - While disabled, it draws dim, with the text kept.
- `class LogView { void setLines(std::vector<DisplayLine> lines); void up(); void down(); bool atTop() const; bool following() const; void follow(); std::string selectedId() const; void render(Grid&, int top, int bottom, int rightReserve, const Theme&) const; };`
  - When following, it shows the newest lines.
  - `up()` from following selects the last message. Further `up()` selects the previous message id, skipping dividers.
  - `down()` on the last message returns to following.
  - On `setLines`, the selected id and the scroll anchor are preserved.
  - Selected lines draw with `selectBg`; mention lines with `mentionBg`.
- `struct ListItem { std::u32string label, right; uint16_t fg; bool marker = false; };` `class ListView { void setItems(std::vector<ListItem>); void up(); void down(); int selected() const; void render(Grid&, int top, int bottom, int left, int width, const Theme&) const; };` It scrolls to keep the selection visible. `marker` draws `•`.
- `class Drawer { void setPeople(std::vector<PresenceEntry> people, int64_t nowMs, int64_t idleAfterMs); void up(); void down(); std::string selectedUsername() const; void render(Grid&, int top, int bottom, const Theme&) const; static int width(TextSize s); };` `width()` returns 15 cols in both sizes: 120 px in Comfortable (8 px per col) and 90 px in Compact (6 px per col), matching the mockups at 1× scale. Header `here · N`. Idle users get a `z` suffix.
- `void renderStatusBar(Grid&, const Theme&, std::u32string_view title, int count, char32_t connMarker, int wifiBars, int batteryPct, std::string_view clock);` Connection markers are `●`, `◌` and `✕`.
- `class Banner { void show(std::u32string text, int64_t nowMs, int64_t durationMs, bool error); bool visible(int64_t nowMs) const; void render(Grid&, int row, const Theme&, int64_t nowMs) const; };`
- `struct PaletteEntry { std::u32string label; std::u32string insert; bool isTextSize = false; };` `const std::vector<PaletteEntry>& paletteEntries();`. The first entry is `Text size: Comfortable/Compact` (`isTextSize`). The rest insert `"/cmd "` for `me slap poke hug hi5 dice 8ball fortune gif art spoiler blink l33t comic cursive times rainbow flip quiet slow glitch wave localtime help muted`.
- `bool qrToCells(std::string_view text, std::vector<std::u32string>& rows);` Uses ricmoo QRCode with ECC_LOW at the smallest version from 1–10 that fits. Two modules per cell vertically, drawn with `█ ▀ ▄ ' '` and a one-module quiet zone. Returns false if the text doesn't fit version 10.

- [x] **Step 1: Write failing tests:**
  - `test_input_insert_cursor`.
  - `test_input_right_at_end_false`.
  - `test_input_byte_cap`: inserting 2,049 ASCII chars leaves 2,048.
  - `test_input_backspace_multibyte`: one emoji is removed whole.
  - `test_input_hscroll`: a cursor beyond the width stays visible.
  - `test_log_follow_and_select`: `up()` selects the last id, `down()` returns to following, and dividers are skipped.
  - `test_log_preserves_selection_on_setLines`.
  - `test_list_scrolls`.
  - `test_drawer_idle_marker`.
  - `test_status_bar_layout`: the title at col 1 and the clock right-aligned.
  - `test_banner_expires`.
  - `test_palette_first_is_text_size`.
  - `test_qr_small_url`: `"https://tenor.com/x"` → true, with rows ≤ 15.
- [x] **Step 2: Run** `pio test -e native -f test_widgets`. Expected: FAIL.
- [x] **Step 3: Implement.**
- [x] **Step 4: Run.** Expected: PASS.
- [x] **Step 5: Commit** `feat(core): text widgets, drawer, palette, QR cells`.

### Task 13: Cell fonts

**Files:** Create `scripts/bdf2cells.py`, `src/ui/fonts/cell6x12.h`, `src/ui/fonts/cell8x16.h`, `src/ui/fonts/OFL-Terminus.txt`, `src/ui/fonts/cell_font.h`, `lib/core/src/ui/glyph_set.h`, `test/test_glyphs/test_main.cpp`

**Interfaces:**
- `struct CellFont { uint8_t w, h; uint16_t count; const uint32_t* codepoints; const uint8_t* bitmaps; };` Code points are sorted. Each glyph is `h` rows, with one byte per row (w ≤ 8), MSB first.
- `const uint8_t* glyphFor(const CellFont& f, char32_t cp);` Binary search. Missing glyphs fall back to `U'?'`'s glyph, or to a box when `0xFFFD` is absent.
- `lib/core/src/ui/glyph_set.h` defines `constexpr char32_t kExtraGlyphs[]`: `─│┌┐└┘├┤┬┴┼ ● ◌ ✕ ▂ ▄ ▆ ▀ █ ▒ · … ← ↑ → ↓ • z ▶ ✓ 🎲(→'*') 🎱(→'8') 🥠(→'f') U+FFFD`. Emoji not in Terminus map to the ASCII stand-ins shown.

- [x] **Step 1: Write `scripts/bdf2cells.py <bdf> <name>`**, adapted from `../rhino-moshi/scripts/bdf2gfx.py`. It emits glyphs for 0x20–0x7E, 0xA0–0xFF and every code point in `glyph_set.h`, padded to the font bounding box, as a `CellFont` header. Source: Terminus 4.49.1 `ter-u12n.bdf` (6×12) and `ter-u16n.bdf` (8×16) from <https://sourceforge.net/projects/terminus-font/>. Download into a fresh directory outside the repo and run the script with `python3 -I`. Commit only the generated headers and the OFL text. The fonts are named `cell6x12`/`cell8x16`, not "Terminus", per the OFL Reserved Font Name.
- [x] **Step 2: Generate both headers.**
- [x] **Step 3: Write failing tests**, compiling the generated headers into the native test via `-Isrc/ui/fonts`:
  - `test_ascii_present`
  - `test_box_drawing_present`
  - `test_missing_falls_back`
  - `test_dims`: 6×12 and 8×16
- [x] **Step 4: Run** `pio test -e native -f test_glyphs`. Expected: PASS after implementing `glyphFor` in `cell_font.h` (header-only).
- [x] **Step 5: Commit** `feat: Terminus-derived 6x12 and 8x16 cell fonts`.

### Task 14: M9 HAL and renderer (hardware bring-up)

**Files:** Create `src/hal/board.h`, `src/hal/m9/board_m9.cpp`, `src/hal/m9/panel_st7789.{h,cpp}`, `src/hal/m9/keyboard_stc8h.{h,cpp}`, `src/ui/renderer.{h,cpp}`. Modify `src/main.cpp`.

**Interfaces:**
- `namespace board { void begin(); const char* name(); int panelWidth(); int panelHeight(); void pushRows(const uint16_t* frame, int y0, int y1); void backlight(bool on); void displaySleep(bool sleep); bool pollKey(uint8_t& raw); bool earlyKeyboard(); int batteryPercent(); bool externalPower(); void beep(int freqHz, int ms); }`
  - `name()` returns `"M9"`.
  - `pushRows` sends rows `[y0, y1)` of the full 320×240 RGB565 frame through CASET/RASET.
  - `begin()`:
    - drives VEXT LOW and waits 50 ms;
    - drives SD CS 48 and LoRa CS 39 HIGH;
    - starts SPI on 40/38/47 at 40 MHz and inits the panel (sequence from rhino-moshi `panel_m9.cpp`: reset, `0x01`, `0x11`, `0x3A 0x55`, MADCTL `0xA0`, `0x21`, `0x13`, `0x29`);
    - attaches backlight PWM on LEDC channel 7, active LOW;
    - releases the USB pads (`gpio_reset_pin(GPIO_NUM_19/20)` plus disabling the USB-Serial-JTAG pull-up, as camillia-mt `m9ReleaseUsbPads()` does);
    - starts `Wire.begin(20, 21, 100000)`;
    - probes 0x6C then 0x6D;
    - reads hw (0x00) and fw (0xFE) to pick the key register (early iff hw == 0x03 and fw == 0x10).
  - `pollKey` reads the key register when INT (GPIO12) is HIGH, or every 20 ms regardless, because early controllers may never raise INT. It returns false for 0x00, 0xFF and 0x88 only if early. Ctrl decoding stays in the keymap.
  - `batteryPercent`: ADC GPIO13 × 2.0, mapped linearly from 3.3 V (0%) to 4.15 V (100%).
- `class Renderer { public: bool begin(); void draw(const Grid& g, TextSize s); };` Owns a 320×240×2 PSRAM frame. For each dirty row, it rasterises the cells with `glyphFor` from the font matching `s`, pushes that pixel band, then calls `g.clearDirty()` on the caller's grid. After a text-size change, the caller marks every row dirty with `fill`.

- [x] **Step 1: Implement the HAL**, porting from `../rhino-moshi/firmware/src/panel_m9.cpp` and `buttons.cpp` (key reading) and `../camillia/camillia-mt/src/keyboard.cpp` (`m9ReleaseUsbPads`).
- [x] **Step 2: Implement `Renderer`.**
- [x] **Step 3: Temporary bring-up `main.cpp`:**
  - draw a 53×20 compact grid containing the full printable ASCII set, the box glyphs and a nick-colour sample;
  - every key prints `[key] raw=0x.. action=..` (via `mapM9Key`) to serial and appears on screen;
  - Ctrl (Palette) toggles Comfortable/Compact;
  - `ScreenOff` turns the backlight off; only another `ScreenOff` (centre hold) wakes it. A centre tap while dark shows a placeholder screensaver for 10 s.
- [x] **Step 4: Run** `pio run -e m9`. Expected: SUCCESS.
- [x] **Step 5: Hardware check (human):** `scripts/build-upload-monitor.sh m9`. Expect:
  - correct orientation, with the keyboard below the screen;
  - crisp glyphs;
  - each of the 6 dedicated buttons, the d-pad, centre, centre-hold, Back-hold and Ctrl log the expected actions;
  - the size toggle redraws;
  - screen off, and wake on centre hold only.

  Record the results in the commit message.
- [x] **Step 6: Commit** `feat(m9): HAL, ST7789 dirty-row renderer, STC8H keyboard`.

### Task 15: Settings and Wi-Fi manager

**Files:** Create `src/store/settings.{h,cpp}`, `src/net/wifi_mgr.{h,cpp}`

**Interfaces:**
- `struct WifiNet { std::string ssid, password; };`
- `struct Settings { std::vector<WifiNet> wifi; std::string email, refreshToken, tz = "UTC0", pagePassword, apPassword; TextSize textSize = TextSize::Compact; bool drawerOpen = false, buzzer = true; std::string lastRoom = "general"; };`
- `namespace settings { void load(Settings&); void save(const Settings&); void saveToken(const std::string& refreshToken); void clearToken(); void factoryReset(); }` (`factoryReset` erases the whole namespace.) Uses NVS namespace `rcfg`, keys ≤ 15 chars, at most 5 Wi-Fi networks.
- `namespace wifimgr { enum class Mode { Station, Setup }; void begin(const Settings&); Mode mode(); bool connected(); int rssiBars(); std::string ip(); std::string apSsid(); bool apHasPassword(); std::vector<std::string> scan(); void reconnectWithBackoff(); }`
  - Setup mode runs when there are no saved networks (first boot, or after a factory reset). There is no boot-time gesture.
  - The SoftAP is `setupApSsid(board::name())`, open unless `apPassword` is non-empty (then WPA2), at 192.168.4.1. **No DNS server.**
  - In station mode it tries saved networks in order.
  - It starts mDNS `rhino-cyberspace` with `_http._tcp` port 80.
  - It runs `configTzTime(tz, "pool.ntp.org", "time.google.com")`.
  - Reconnect backoff is 1, 2, 4 … 60 s.

- [x] **Step 1: Implement** (no native tests: thin wrappers over NVS and Wi-Fi).
- [x] **Step 2: Wire into `main.cpp`** temporarily: load settings, then `wifimgr::begin`. Print the mode, the AP SSID (and whether a password is set, never the password itself), and the IP.
- [x] **Step 3: Run** `pio run -e m9`. Expected: SUCCESS.
- [x] **Step 4: Hardware check (human):**
  - first boot shows an open `Rhino-Cyberspace-M9-Setup` that a phone can join, with nothing popping up automatically;
  - (user decision) no boot gesture forces setup; a factory reset (Tasks 19 and 21) does.
- [x] **Step 5: Commit** `feat: NVS settings and Wi-Fi manager (open setup AP, no captive portal)`.

### Task 16: HTTPS client and Cyberspace API layer

**Files:** Create `src/net/https_client.{h,cpp}`, `src/cs/cs_api.{h,cpp}`

**Interfaces:**
- `struct HttpResult { int status = 0; std::string body; bool transportError = false; };`
- `class HttpsClient { public: HttpResult request(const char* method, const std::string& path, const std::string& jsonBody, const std::string& bearer); void close(); };`
  - Base `https://api.cyberspace.online`, using `esp_http_client` with `crt_bundle_attach = esp_crt_bundle_attach`, keep-alive, a 15 s timeout, and a 256 KB body cap.
  - It sends `Content-Type: application/json` and `User-Agent: rhino-cyberspace/<RC_VERSION>`.
- `class CsApi { public: explicit CsApi(HttpsClient&); … };` The token methods are:

  | Method | Behaviour |
  |---|---|
  | `ApiError login(const std::string& email, const std::string& password, Tokens& out)` | Login |
  | `ApiError refresh(Tokens& io)` | Refresh |
  | `ApiError resendVerification(const std::string& idToken)` | Resend the verification email |
  | `bool ensureFresh(int64_t monoMs, int64_t epochSec)` | Refreshes when `monoMs >= refreshDue` |

  The cIRC methods take the room slug and are bearer-authenticated through `tokens()`:

  | Method | Request |
  |---|---|
  | `rooms(std::vector<RoomInfo>&)` | `GET /v1/circ` |
  | `history(slug, int64_t before, std::vector<CircMessage>&, int64_t& cursor)` | `GET /v1/circ/:slug?limit=50[&before=]` |
  | `send(slug, utf8, std::string& messageId, std::string& reply)` | `POST /v1/circ/:slug` `{"content":…}` |
  | `deleteMessage(slug, id)` | `DELETE /v1/circ/:slug/messages/:id` |
  | `flag(slug, id, reason)` | `POST …/flag` `{"reason":…}`, reason omitted when empty |
  | `markRead(slug)` | `POST /v1/circ/:slug/read` |
  | `users(slug, std::vector<PresenceEntry>&)` | `GET /v1/circ/:slug/users` |
  | `presence(slug, int64_t lastActivity, PresenceCadence&)` | `POST /v1/circ/:slug/presence` `{"lastActivity":…}` |
  | `leave(slug)` | `DELETE /v1/circ/:slug/presence` |

  The notify methods are:

  | Method | Request |
  |---|---|
  | `mentions(std::vector<Mention>&)` | `GET /v1/notifications?read=false&type=chat_mention&limit=20` |
  | `markNotificationRead(id)` | `PATCH /v1/notifications/:id` |

  All of them return `ApiError` and parse with the Task 8 parsers.
  - Writes check their `RateLimiter` first. When a limiter refuses, they return `ApiError{429, "LOCAL_RATE_LIMIT", "Slow down: try in Ns"}` without a request.
  - On `401` they refresh once and retry the same call once. That retry is allowed for auth expiry only. No retry follows a successful write that returned 5xx.
- Accessors: `const Tokens& tokens() const; bool signedIn() const; void signOut();` `signOut` clears the tokens and calls `settings::clearToken()`.
- Also `void setRefreshToken(const std::string&)` (restores a session from NVS at boot) and `void expireNow()` (forces the next `ensureFresh` to refresh, for RTDB `auth_revoked`). The resend-verification limiter is 1/min and 5/hour; `leave` shares the presence limiter.

- [x] **Step 1: Implement** both.
- [x] **Step 2: Run** `pio run -e m9`. Expected: SUCCESS.
- [x] **Step 3: Grep check:** `grep -rn "idToken\|refreshToken\|password" src | grep -i "print\|log"` returns nothing that logs a value.
- [x] **Step 4: Commit** `feat: HTTPS client and Cyberspace API layer with local rate limits`.

### Task 17: SSE client

**Files:** Create `src/net/sse_client.{h,cpp}`

**Interfaces:**
- `class SseClient { public: using OnEvent = std::function<void(const SseEvent&)>; using OnState = std::function<void(bool live)>; bool start(const std::string& url, OnEvent onEvent, OnState onState); void stop(); bool running() const; };`
  - `start` spawns one task on core 0 (8 KB stack, internal RAM; read buffer 4 KB in PSRAM).
  - It sends `GET` with `Accept: text/event-stream` and `esp_http_client` in streaming mode (`esp_http_client_open` + `esp_http_client_read`), feeding the `SseParser`.
  - `onState(true)` fires on the first byte; `onState(false)` on close.
  - It does **not** reconnect by itself. The owner decides, because reconnecting needs a fresh token and `startAfter`.
  - `stop()` closes the connection and joins the task within 2 s.
  - It follows up to 3 redirects (RTDB may answer a stream with `307`), reads in 500 ms slices so `stop()` is prompt, and closes after 75 s with no bytes (RTDB keep-alives come every ~30 s). The URL carries the token and is never logged.

- [x] **Step 1: Implement.**
- [x] **Step 2: Run** `pio run -e m9`. Expected: SUCCESS.
- [x] **Step 3: Commit** `feat: streaming SSE client task`.

### Task 18: App messages, queues, chat source, net worker, fake source

**Files:** Create `src/app/messages.h`, `src/app/queues.{h,cpp}`, `src/store/locked_chat.{h,cpp}`, `src/chat/chat_source.h`, `src/chat/net_chat_source.{h,cpp}`, `src/app/net_worker.{h,cpp}`, `src/chat/fake_chat_source.{h,cpp}`

**Interfaces:**
- `messages.h`:
  - `enum class CmdType { OpenRoom, CloseRoom, Send, LoadOlder, Delete, Flag, MarkRead, OpenPresence, ClosePresence, RefreshRooms, Login, Logout, ResendVerification, ScreenOff, ScreenOn, Activity };`
  - `struct Command { CmdType type; std::string a, b; int64_t n = 0; };`
  - `enum class EvtType { RoomChanged, RoomsUpdated, PresenceUpdated, ConnState, Banner, MentionAlert, AuthState, SendDone, OlderLoaded };`
  - `struct Event { EvtType type; std::string text; int code = 0; bool ok = true; };`
  - `ConnState` code: 0 offline, 1 reconnecting, 2 live. `AuthState` code: 0 signed out, 1 signed in, 2 email unverified.
- `queues.h`: `bool postCommand(Command c); bool nextCommand(Command& c, uint32_t waitMs); bool postEvent(Event e); bool nextEvent(Event& e);` Depth 32 each. Events beyond capacity drop the oldest `RoomChanged` first.
- `locked_chat.h`: `class LockedChat { public: template<class F> auto with(F f);` runs `f(ChatStore&, PresenceMap&, std::vector<RoomInfo>&)` under a `std::mutex`. Also `std::string roomSlug; std::set<std::string> readMarkers;` Read markers map slug → last read `lastMessageAt`, stored as `std::map<std::string,int64_t> readAt`.
- `chat_source.h`: `class ChatSource { public: virtual ~ChatSource() = default; virtual void command(const Command&) = 0; };` Screens call `chatSource().command(...)`. `ChatSource& chatSource();` is defined by whichever implementation is linked.
- `NetChatSource::command` → `postCommand`.
- `net_worker` (task on core 0, 12 KB stack) loops on `nextCommand(…, 200)` and handles timers.
  - **OpenRoom:**
    1. Close the existing streams and send `DELETE presence` for the old room.
    2. Clear the store.
    3. Run `history` → `replaceAll`.
    4. Start an SSE on `chat_messages/<slug>` with `{true, newestTs, 1000}`. Events go to `applyChatEvent` under the lock, then post `RoomChanged`.
    5. Send `presence` now and every `heartbeatMs`, with `lastActivity` from the latest `Activity` command.
    6. Mark read.
  - **SSE close or `Cancel`/`AuthRevoked`:** post `ConnState 1`, call `ensureFresh` (forced on `AuthRevoked`), and reopen with `startAfter = store newest` after a backoff of 1, 2, 4 … 30 s. After 30 s down, post `Banner "Chat paused, reconnecting"`.
  - **Send:** call `send`. On success post `SendDone ok`, plus `Banner(reply)` when `reply` is non-empty. On failure post `SendDone !ok` with `text = message`.
  - **LoadOlder:** call `history(before = oldest)`, upsert, post `OlderLoaded`.
  - **OpenPresence / ClosePresence:** start or stop the second SSE on `chat_presence/<slug>` (no query). Seed from `users`.
  - **ScreenOff:** stop both streams and leave. **ScreenOn:** reopen the current room.
  - **Watcher:** every 60 s when signed in:
    - `rooms` → `RoomsUpdated`;
    - `mentions` → for ids not seen before, `MentionAlert(text = "@actor in #room: content")`;
    - after the room is opened at the bottom, mark that room's mentions read.
  - **Login/Logout/ResendVerification:** call `CsApi`, persist the token, post `AuthState`. Map `EMAIL_NOT_VERIFIED` to code 2.
  - **Wi-Fi down:** post `ConnState 0`. Commands that need the network return `Banner "Offline"` and `SendDone !ok`.
- `fake_chat_source.cpp` (only in `m9_fake`, guarded by `#if RC_FAKE_API`):
  - implements `chatSource()` by applying commands directly to `LockedChat`, with fixture rooms (`general`, `linux`, `music`);
  - streams a scripted message every 4 s, including an action, dice, a spoiler, art, a deletion patch and a mention of `oumike`;
  - presence of 12 users, 3 idle;
  - a send that echoes after 300 ms;
  - `/dice x` returns a 400 banner;
  - the 16th send in a minute returns the local rate-limit banner.
  - `main.cpp` skips Wi-Fi and the net worker in this env and reports signed in as `oumike`.

- **As built (refinements):**
  - `LockedChat::with(f)` passes one `ChatState&` holding `store`, `presence`, `rooms`, `cadence`, `roomSlug`, `self`, `readAt` (slug → `lastMessageAt`), `revealed` and `noOlder`. `lockedChat()` returns the instance.
  - `chat_source.h` also declares `void startChatSource(const Settings&)`; the net build starts Wi-Fi and the worker there, the fake build its script task. The two sources are `.cpp` files only.
  - The REST and RTDB paths take the room **id** (docs: `:roomId`, `chat_messages/<roomId>`); the worker maps slug → id from `GET /v1/circ` and falls back to the slug.
  - The worker learns the username from `GET /v1/users/me` (`CsApi::me`) and saves logins with `settings::saveAccount(email, refreshToken)`. A 401 that survives a refresh, or a 400/401 from the refresh itself, signs out (`AuthState 0`).
  - An empty room opens its stream with `limitToLast=50` instead of 1000.
- [x] **Step 1: Implement** the messages, queues, `LockedChat` and the source interface.
- [x] **Step 2: Implement** `net_worker` and `NetChatSource`.
- [x] **Step 3: Implement** `FakeChatSource`.
- [x] **Step 4: Run** `pio run -e m9 && pio run -e m9_fake`. Expected: both SUCCESS.
- [x] **Step 5: Commit** `feat: net worker, chat source abstraction, fake source for m9_fake`.

### Task 19: Screens, router, navigation

**Files:** Create `src/screens/router.{h,cpp}`, `src/screens/{splash,setup,home,room_list,coming_soon,login_form}.{h,cpp}`, `src/app/ui_task.{h,cpp}`. Modify `src/main.cpp` to start the tasks.

**Interfaces:**
- `class Screen { public: virtual ~Screen() = default; virtual void enter() {} virtual void leave() {} virtual void onKey(const KeyEvent&) = 0; virtual void onEvent(const Event&) {} virtual void render(Grid&, const Theme&, int64_t nowMs) = 0; };`
- `namespace router { enum class Id { Splash, Setup, Home, RoomList, Room, ComingSoon, LoginForm }; void go(Id id, const std::string& arg = ""); Id current(); void handleGlobal(const KeyEvent&); }`
  - Global keys:
    - `OpenCirc`: Room for `settings.lastRoom`, or RoomList if already in Room with no overlay.
    - `OpenHome`: Home.
    - `OpenCmail`: ComingSoon "C-Mail".
    - `OpenFeed`: ComingSoon "Feed".
    - `OpenMap`: ignored.
  - Each of these closes overlays first.
- `ui_task` (core 1, 16 KB stack) runs every 33 ms:
  - poll the key, map it, apply screen-off gating;
  - send an `Activity` command at most every 5 s;
  - drain events;
  - render the active screen to the Grid;
  - `Renderer::draw`.

  Other duties:
  - `MentionAlert` → `board::beep(2000, 80)` when the buzzer is on, plus a banner.
  - `ScreenOff` action → `backlight(false)`, `displaySleep(true)`, `Command ScreenOff`.
  - Wake (centre hold) → `ScreenOn`.
  - Centre tap while dark → the **Screensaver** for 10 s, drawn from the stored messages of the current room (no command, no network), then dark again. Replaces the Task 14 placeholder.
- Screens:
  - **Splash:** name, version, "connecting…".
  - **Setup:** the AP SSID, "open network" or "password required", `http://192.168.4.1`, and once on Wi-Fi `http://rhino-cyberspace.local` plus the IP. Entry "Log in on this device" → LoginForm.
  - **LoginForm:** email field, then password field (masked `•`). Select submits a `Login` command and shows the result.
  - **Home:** user, Wi-Fi SSID and IP, the settings URL, unread mentions count, rooms with new activity, battery, and a last entry **Factory reset…**: a confirm menu ("Cancel" default, "Erase everything") that calls `settings::factoryReset()` and restarts.
  - **RoomList:** `ListView` of rooms sorted by `sortOrder` then `lastMessageAt`, with a `•` marker when `lastMessageAt > readAt[slug]` and the online count on the right. Select/Right opens the room; Left/Back goes Home.
  - **ComingSoon:** the feature name and "coming in a later version".
- Boot flow:
  - no Wi-Fi saved → Setup;
  - else signed out → Setup with a "Sign in at rhino-cyberspace.local or on this device" line;
  - else → Home.

- [ ] **Step 1: Implement** the router, ui_task and the screens above.
- [ ] **Step 2: Run** `pio run -e m9_fake`. Expected: SUCCESS.
- [ ] **Step 3: Hardware check (human) on `m9_fake`:** boots to Home; Messages → `#general`; Messages again → room list with markers; Fn keys → Coming soon; Home → Home; Map does nothing; centre-hold → screen off; other keys ignored while off; centre tap → screensaver with the last messages, dark again after 10 s; centre-hold wakes.
- [ ] **Step 4: Commit** `feat: router, home, room list, setup and login screens`.

### Task 20: Room screen

**Files:** Create `src/screens/room.{h,cpp}`

**Interfaces:**
- `class RoomScreen : public Screen`. It uses `LockedChat`, `layoutRoom`, `LogView`, `InputLine`, `Drawer`, `ListView` (menus), `Banner`, `paletteEntries()` and `qrToCells`. The text size comes from `Settings`.

**Behaviour (spec §5–§7):**
- **Layout:**
  - Row 0 is the status bar.
  - Rows 1 to rows−3 are the log in Compact; to rows−2 in Comfortable.
  - The next row is the input line.
  - In Compact, the last row is the hint row, with text by context:
    - typing: `● send  ↑↓ scroll  → who's here  ctrl cmds`
    - drawer open: `↑↓ pick  ● menu  ← close`
    - a message selected: `● actions  ↓ back to typing`
  - Banners overwrite the hint row, or the input row in Comfortable, until they expire (4 s; 6 s for errors).
- **Typing:**
  - `Char` → insert; `Backspace` → delete; `ClearLine` → clear.
  - `Select` with text → `Send`. The text moves to a dim `sending…` pending line in the log until `SendDone`. On `!ok`, the text is restored to the input and the banner shows the error.
  - `Select` on an empty line → room menu: Who's here, Mute…/Unmute…, Mark read, Text size, Leave room. Mute and Unmute send `/mute name` and `/unmute name`.
- **Navigation:**
  - Up/Down → `LogView`. Up at the top sends `LoadOlder` once per top hit.
  - Left/Right → cursor. A `Right` when `right()` returns false toggles the drawer: `OpenPresence`/`ClosePresence`, saved in `settings.drawerOpen`.
  - Drawer open: Up/Down pick; Select → person menu (Mention → inserts `@name `, Mute/Unmute); Left or Back closes it.
  - Select on a selected message → message menu:
    - Reply → `@name `.
    - Show full or Reveal: a full-screen reader for long or art messages; reveal adds the id to `revealed`.
    - Show link: URL text plus QR. If the QR doesn't fit, the URL only.
    - Delete: own messages only, with a confirm dialog, then `Delete`.
    - Report: confirm, with an optional reason typed into the input line, then `Flag`.
  - `Palette` → palette overlay. Selecting the text-size entry toggles and saves the size and redraws everything. Any other entry inserts its text.
  - Back closes overlays in order (menu → drawer → selection). With nothing open, Back goes to RoomList.
- **Read state:** when the log is following and the window is visible, send `MarkRead` at most every 10 s, and update `readAt[slug]`.
- **Offline:** `ConnState 0` disables the input (dim) and blocks Send with `Banner "Offline: not sent"`.

- [ ] **Step 1: Implement.**
- [ ] **Step 2: Run** `pio run -e m9_fake && pio run -e m9`. Expected: SUCCESS.
- [ ] **Step 3: Hardware check (human) on `m9_fake`:** every item in Task 22's live checklist that does not need the real server, including both text sizes, the drawer in each, the palette, menus, a QR on a GIF link, the `/dice` error banner, and the local rate-limit banner on the 16th fast send.
- [ ] **Step 4: Commit** `feat: cIRC room screen`.

### Task 21: Web config server

**Files:** Create `src/web/web_server.{h,cpp}`, `src/web/page.h`

**Interfaces:**
- `namespace web { void begin(); void stop(); }` uses `esp_http_server` on port 80, started in both Wi-Fi modes. Every handler checks `checkBasicAuth(header, settings.pagePassword)` and returns 401 with `WWW-Authenticate: Basic realm="rhino-cyberspace"` on failure.
- Routes (JSON bodies):

  | Route | Behaviour |
  |---|---|
  | `GET /` | `page.h`: one inline HTML/CSS/JS page, no external requests |
  | `GET /api/status` | `{version, board, mode, ip, ssid, rssiBars, battery, signedIn, email, conn}` |
  | `GET /api/wifi/scan` | `{networks:[ssid…]}` |
  | `POST /api/wifi` | `{ssid,password}` adds or updates (max 5) |
  | `DELETE /api/wifi?ssid=` | Removes a network |
  | `POST /api/login` | `{email,password}` → posts a `Login` command, waits up to 20 s for `AuthState`, returns `{ok, code, message}`. The password is not stored or logged; its buffer is zeroed after the request |
  | `POST /api/logout` | Signs out |
  | `POST /api/resend` | Resends the verification email |
  | `GET/POST /api/prefs` | `{textSize:"compact"|"comfortable", buzzer, tz}` |
  | `POST /api/security` | `{pagePassword?, apPassword?}` validates with `validApPassword`; 400 with a message on failure |
  | `POST /api/restart` | Restarts the device |
  | `POST /api/factory-reset` | `{confirm:true}` required (else 400). Calls `settings::factoryReset()` and restarts. The page asks for confirmation first |

- Page sections: Status, Wi-Fi, Cyberspace account, Display, Security. The security notes are copied verbatim from spec §8, the paragraph beginning "the page is plain HTTP on your LAN". The TZ dropdown includes `UTC0`, `EST5EDT,M3.2.0,M11.1.0`, `CST6CDT,M3.2.0,M11.1.0`, `MST7MDT,M3.2.0,M11.1.0`, `PST8PDT,M3.2.0,M11.1.0`, `GMT0BST,M3.5.0/1,M10.5.0`, `CET-1CEST,M3.5.0,M10.5.0/3`, `AEST-10AEDT,M10.1.0,M4.1.0/3`, and a free-text field.
- Applying Wi-Fi from setup mode saves and restarts into station mode.

- [ ] **Step 1: Implement.**
- [ ] **Step 2: Run** `pio run -e m9`. Expected: SUCCESS.
- [ ] **Step 3: Hardware check (human):** join the open setup AP, browse to `http://192.168.4.1` (no popup appears), add Wi-Fi, and confirm the device restarts onto it. Then check `http://rhino-cyberspace.local` loads, setting a page password triggers Basic auth, and setting and clearing an AP password works (WPA2 when set, open when cleared).
- [ ] **Step 4: Commit** `feat: device web config (Wi-Fi, login, prefs, security)`.

### Task 22: Docs, live checklist, CI, agent notes

**Files:** Create `docs/USE.md`, `docs/TESTING.md`, `.github/workflows/build.yml`, `AGENTS.md`, `.github/copilot-instructions.md` (a two-line pointer to `AGENTS.md`), `README.md`

- [ ] **Step 1: `docs/USE.md`:** first setup (open AP, the address to browse, login), the button map (spec §5 table), room controls, text sizes and drawer, alerts, the security notes, and returning to setup mode with a factory reset (Home screen or web config).
- [ ] **Step 2: `docs/TESTING.md`:** the host test command, the `m9_fake` procedure, and the **live checklist (human only)** from spec §10.3 as checkboxes:
  - web login
  - join a room
  - send plain, `/me` and a bad `/dice`
  - scroll back past the first page
  - drawer shows yourself
  - a mention from the website with the screen off → buzzer
  - delete your own message → tombstone on the website
  - Wi-Fi off/on
  - more than 70 minutes of uptime across a token refresh
- [ ] **Step 3: `.github/workflows/build.yml`:** on push and PR. Use `actions/setup-python@v5`, `pip install platformio`, `pio test -e native`, `pio run -e m9 -e m9_fake`, with a cache of `~/.platformio`.
- [ ] **Step 4: `AGENTS.md`:**
  - the project summary;
  - links to the spec and this plan;
  - the Global Constraints above, especially "never call the live API from an agent", "no captive portal" and the LoRa gateway note from spec §12;
  - build and test commands;
  - "work tasks in order, tick plan checkboxes, one commit per task".
- [ ] **Step 5: Run** `pio test -e native && pio run -e m9 -e m9_fake`. Expected: all PASS / SUCCESS.
- [ ] **Step 6: Commit** `docs: usage, testing checklist, CI, agent instructions`.

---

## Spec coverage check

| Spec section | Tasks |
|---|---|
| §1 success 1–2 (setup page, login) | 15, 19, 21 |
| §1 success 3 (rooms, history, live) | 7, 8, 16, 17, 18, 19, 20 |
| §1 success 4 (send, commands, banners) | 9, 16, 18, 20 |
| §1 success 5 (scrollback, menus, who's here) | 11, 12, 18, 20 |
| §1 success 6 (presence) | 7, 16, 18 |
| §1 success 7 (alerts) | 18, 19 |
| §1 success 8 (token, Wi-Fi resilience) | 9, 15, 16, 17, 18 |
| §5 controls | 10, 19, 20 |
| §6 screens | 11, 12, 13, 14, 19, 20 |
| §8 web config | 10, 15, 21 |
| §9 errors | 8, 9, 16, 18, 20 |
| §10 testing | every core task; 18 (`m9_fake`); 22 |
| §12 future | 18 (`ChatSource` seam), 22 (`AGENTS.md`) |
