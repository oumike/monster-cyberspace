# Cardputer ADV Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A rooms-only firmware env `cardputer_adv` for the M5Stack Cardputer ADV (ESP32-S3, 8 MB flash, no PSRAM).

**Architecture:** Nine compile-time feature flags (`RC_FEAT_*`, default 1) guard each feature's wiring points; the new env turns them all off. A new HAL in `src/hal/cardputer_adv/` reuses the shared `st7789` driver, the Pager's TCA8418 path and the Wio/Pager `es8311` driver; portable logic (feature-dependent tile list, keymap, splash fit) lives in `lib/core` with native tests.

**Tech Stack:** PlatformIO, Arduino-ESP32 3.x, C++17, Unity native tests.

**Spec:** `docs/superpowers/specs/2026-11-01-cyberspace-cardputer-adv-design.md`

## Global Constraints

- Read `AGENTS.md` first; its hard rules bind every task (never call the live Cyberspace API; every write from a key press; no captive portal; never log passwords or tokens).
- Hardware access outside `src/hal/` only through `src/hal/board.h`; never include `src/hal/<board>/` headers elsewhere.
- Every `RC_FEAT_*` defaults to 1; with all of them 1, every existing board's behaviour is unchanged.
- The flags: `RC_FEAT_CMAIL`, `RC_FEAT_FEED`, `RC_FEAT_GUILDS`, `RC_FEAT_PEOPLE`, `RC_FEAT_UNREAD`, `RC_FEAT_SPELL`, `RC_FEAT_EMOJI`, `RC_FEAT_OTA`, `RC_FEAT_MASCOT`, defined in `lib/core/src/features.h`.
- A feature's own `.cpp` files wrap their whole body in `#if RC_FEAT_X` (as HAL files do with `#if defined(BOARD_X)`); no `build_src_filter`.
- Board macro `BOARD_CARDPUTER_ADV`; env name `cardputer_adv`; release file slug `cardputer-adv`; flasher label "M5Stack Cardputer ADV".
- Pins (spec §2): panel SPI3 SCK 36, MOSI 35, CS 37, DC 34, RST 33, BL 38, 240×135, offsets X 53 / Y 40, inverted; TCA8418 I²C SDA 8, SCL 9, addr 0x34, INT 11; ES8311 addr 0x18, I²S BCLK 41, LRCK 43, DOUT 42, no MCLK; battery ADC GPIO 10; SD SCK 40, MOSI 14, MISO 39, CS 12.
- Partition table `partitions_8mb.csv`: factory 0x10000 size 0x7C0000; nvs 0x7D0000 size 0x20000; coredump 0x7F0000 size 0x10000.
- Build with `nice`. Never flash. Commit on `main` with the trailer `Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>`. Do not push.
- Commands: `nice pio test -e native`, `nice pio run -e <env>`.

## Review Focus

- A room message containing a wide emoji with emoji off: it must keep its two-cell width and draw a placeholder, never garble the line. (Task 3 test.)
- A key that the keymap doesn't know (Fn alone, Ctrl, Opt, Alt): it must produce no key, not a stray character. (Task 2 test.)
- A mention or link in a room pointing at a profile or C-Mail with People/C-Mail off: activating it must do nothing harmful (no navigation to a missing screen). (Task 5 grep-check step.)
- Settings backup restored from another board's file that contains OTA/mascot/text-size keys: restore must ignore what this board lacks and keep Compact. (Task 4 step.)
- A splash on 40×11 with a long version string or a long bubble phrase: nothing may wrap onto the art or run off the grid. (Task 3 test.)

---

### Task 1: Feature flags and Home's tile list

**Files:**
- Create: `lib/core/src/features.h`, `lib/core/src/ui/home_items.h`, `lib/core/src/ui/home_items.cpp`
- Modify: `src/screens/home.cpp` (replace the fixed `kTiles[]` array, lines ~21-22, and everything indexed by `kTileCount`)
- Test: `test/test_home_items/test_main.cpp`

**Interfaces:**
- Produces: `features.h` with the nine `#ifndef RC_FEAT_X / #define RC_FEAT_X 1` blocks, and
  `struct rc::Features { bool cmail, feed, guilds, people, unread; };`,
  `constexpr rc::Features rc::kBuiltFeatures` (from the macros),
  `std::vector<HomeItem> rc::homeItems(const Features& f)` where `enum class rc::HomeItem { Unread, Messages, Rooms, Cmail, Feed, Guilds, Settings }`.

- [ ] **Step 1: Write the failing tests**: `test_all_features_keeps_todays_order` (all true → `Unread, Messages, Rooms, Cmail, Feed, Guilds, Settings`); `test_rooms_only` (all false → `Messages, Rooms, Settings`); `test_unread_off_only_drops_unread`. ("Messages" is today's last-room tile, part of rooms.)
- [ ] **Step 2: Run** `nice pio test -e native -f test_home_items`. Expected: build failure (missing header).
- [ ] **Step 3: Implement** `features.h` and `homeItems()`; switch `home.cpp` to build its tiles from `homeItems(kBuiltFeatures)` (map `HomeItem` to its existing `Item`), with tile count from the vector's size.
- [ ] **Step 4: Run** `nice pio test -e native`. Expected: all pass. Then `nice pio run -e m9`. Expected: SUCCESS.
- [ ] **Step 5: Commit** `git commit -m "Feature flags and a Home tile list that follows them"`

### Task 2: Cardputer ADV keymap

**Files:**
- Create: `lib/core/src/input/keymap_cardputer_adv.h`, `.cpp`
- Test: `test/test_keymap_cardputer_adv/test_main.cpp`

**Interfaces:**
- Produces: `class rc::CardputerKeys { public: KeyEvent key(uint8_t keyNum, bool pressed, uint32_t ms); KeyEvent poll(uint32_t ms); };` — same shape as `PagerKeys` (`lib/core/src/input/keymap_pager.h`), taking TCA8418 FIFO key numbers.

The TCA8418 key-number → physical-key table and the Fn layer come from M5Cardputer 1.1.x's TCA8418 reader (`https://github.com/m5stack/M5Cardputer`, `src/utility/Keyboard/KeyboardReader/TCA8418.*` and the shared key map `KeyboardReader.h`/`Keyboard_def.h`). Fetch those files read-only and copy the mapping into a table with a comment citing the file and tag. Rules: Shift and Fn are held modifiers (pressed while the key is pressed, unlike the Pager's one-shot); Fn+`;` Up, Fn+`.` Down, Fn+`,` Left, Fn+`/` Right; the `` ` `` key (Esc) is Back; Enter is Select; Backspace deletes and repeats like the Pager's (same delay/interval constants); Fn+Backspace is Home (as Sym+Backspace on the Pager); Ctrl, Opt and Alt alone produce nothing.

- [ ] **Step 1: Write the failing tests**: `test_letters_and_shift` (`a`, Shift+`a` → `A`); `test_fn_arrows` (the four arrows); `test_esc_is_back`; `test_enter_selects`; `test_backspace_repeats` (held 500 ms then every 70 ms via `poll`); `test_fn_backspace_is_home`; `test_modifiers_alone_send_nothing` (Fn, Shift, Ctrl, Opt, Alt presses and releases → `Action::None`); `test_release_sends_nothing`.
- [ ] **Step 2: Run** `nice pio test -e native -f test_keymap_cardputer_adv`. Expected: build failure.
- [ ] **Step 3: Implement** `CardputerKeys`.
- [ ] **Step 4: Run** `nice pio test -e native -f test_keymap_cardputer_adv`. Expected: PASS.
- [ ] **Step 5: Commit** `git commit -m "Cardputer ADV keymap"`

### Task 3: Splash fit on small grids and the emoji placeholder

**Files:**
- Modify: `lib/core/src/ui/splash_layout.cpp` (and `.h` if needed)
- Modify: `lib/core/src/ui/text_width.h` or a new `lib/core/src/ui/emoji_placeholder.h`; `src/ui/renderer.cpp` (draw path for wide emoji when `RC_FEAT_EMOJI` is 0)
- Test: `test/test_splash_layout/` (existing; add cases), `test/test_emoji_placeholder/test_main.cpp`

**Interfaces:**
- Produces: `char32_t rc::emojiPlaceholder()` (returns `U'?'`-like glyph Terminus has; pick one in `ui/glyph_set.h`, e.g. U+25A1 `□` if present, else `?`), used by the renderer for both cells' left/right halves: left cell draws the placeholder, right (tail) cell draws a blank.

- [ ] **Step 1: Write the failing tests**: in the splash test, `test_fits_40x11` (every line's row in [0,11), every line's `col + width ≤ 40`, art rows don't overlap other roles), `test_drops_detail_then_bubble_when_short` (rows 9 → no Detail; rows 7 → no Bubble either), `test_long_phrase_never_overruns` (80-char phrase at 40 cols); `test_emoji_placeholder_is_one_narrow_glyph` (width of placeholder is 1 and it's in the cell font's set per `ui/glyph_set.h`).
- [ ] **Step 2: Run** them. Expected: the new cases fail.
- [ ] **Step 3: Implement** the drop order (Detail first, then Bubble) in `splashLayout`, and the placeholder; the renderer uses it only under `#if !RC_FEAT_EMOJI`.
- [ ] **Step 4: Run** `nice pio test -e native`. Expected: all pass.
- [ ] **Step 5: Commit** `git commit -m "Splash fits small grids; emoji placeholder"`

### Task 4: The board: env, partitions, HAL, text-size hook

**Files:**
- Create: `partitions_8mb.csv`; `src/hal/cardputer_adv/` — `pins_cardputer_adv.h`, `board_cardputer_adv.cpp`, `keyboard_cardputer_adv.cpp/.h`, `sd_cardputer_adv.cpp`
- Modify: `platformio.ini` (new `[env:cardputer_adv]`), `src/hal/board.h` + `src/hal/board_defaults.cpp` (`fixedTextSize`), `src/hal/m9/panel_st7789.cpp` and `src/hal/wio_l2/audio_es8311.cpp/.h` (add `BOARD_CARDPUTER_ADV` to their `#if` and the BCLK clock option), `src/app/ui_task.cpp` (`wantedSize`, `start`), `src/screens/settings.cpp` and `src/web/page.h`/`web_server.cpp` (hide text size when fixed), `src/store/settings.cpp` or the backup restore path (keep the fixed size after a restore)

**Interfaces:**
- Consumes: `CardputerKeys` (Task 2).
- Produces: `bool board::fixedTextSize(TextSize& out)` — default returns false; this board sets `out = TextSize::Compact` and returns true. `es8311::Config` gains `bool mclkFromBclk` (default false; when true, program register 0x01 and 0x02 as M5Unified does for the Cardputer ADV — 0x01 = 0xB5 (speaker path), 0x02 = 0x18 — and pass no MCLK pin to I²S).

Env: `board = esp32-s3-devkitc-1`, `board_build.flash_size = 8MB`, `board_upload.flash_size = 8MB`, `board_build.arduino.memory_type = qio_qspi`, `board_build.partitions = partitions_8mb.csv`, build flags `-DBOARD_CARDPUTER_ADV -DARDUINO_USB_MODE=1 -DARDUINO_USB_CDC_ON_BOOT=1 -DRC_LOW_MEM=1 -DRC_CHAT_CAP=150 -std=gnu++17`, no `BOARD_HAS_PSRAM`, same `extra_scripts` as the x4 env minus nothing (spell/mascot scripts stay until Task 6). Copy the structure of the `x4` and `pager` envs.

HAL: follow `src/hal/pager/` (keyboard over TCA8418 with `CardputerKeys`, I²C on SDA 8 / SCL 9, INT 11) and the shared `st7789` driver's `Config` (240×135 landscape, offsets 53/40 — set MADCTL for landscape; the exact value awaits the hardware check, note it in a comment), backlight PWM on GPIO 38, battery from ADC1 GPIO 10 with a ×2 divider mapped like the other ADC boards, SD on its own SPI bus (SCK 40, MOSI 14, MISO 39, CS 12) mounted per operation like `sd_pager.cpp`, beeps through `es8311` with `mclkFromBclk = true`, no amp callback. Flags: `hasKeyboard`/`canType`/`hasKeys` true; touch, rotate, eink, notify light, GPS, RTC false. `otaSlug()` returns `"cardputer-adv"`. Free heap is printed on the console after sign-in (one `Serial.printf`, `RC_LOW_MEM` builds only).

- [ ] **Step 1: Add the env and partitions; run** `nice pio run -e cardputer_adv`. Expected: link fails on missing `board::` functions.
- [ ] **Step 2: Implement the HAL and `fixedTextSize`**; in `ui_task.cpp` `wantedSize()` returns the fixed size for every screen (splash and screensaver too) when `fixedTextSize` is true, and `start()` skips the `RC_LOW_MEM` Compact→Comfortable rewrite in that case.
- [ ] **Step 3: Hide the text-size item** in device Settings and web config when the size is fixed; a backup restore that carries a text size leaves the fixed size in effect.
- [ ] **Step 4: Build** `nice pio run -e cardputer_adv`, `nice pio run -e pager`, `nice pio run -e wio_l2`. Expected: all SUCCESS. Report the cardputer RAM/flash summary lines.
- [ ] **Step 5: Commit** `git commit -m "Cardputer ADV board, 8 MB layout, fixed text size"`

### Task 5: Compile out C-Mail, Feed, Guilds, People and Unread

**Files:**
- Modify (guards at wiring points): `src/screens/router.cpp` (screen switch, hot keys at ~lines 120-150), `src/screens/screens.h` (screen object declarations), `src/main.cpp`, `src/app/ui_task.cpp` (feed/people workers, `cmailUnread`, guild posting, mention/unread handling), `src/app/net_worker.cpp` (DM channels `dm_`/`dm_presence`), `src/app/feed_worker.cpp`, `src/app/people_worker.cpp`, `src/screens/room.cpp` (links into profiles and C-Mail), `src/screens/settings.cpp`, `src/web/web_server.cpp` and `src/web/page.h` (feature sections)
- Wrap whole files: `src/screens/cmail_list.cpp`, `feed_list.cpp`, `entry.cpp`, `composer.cpp`, `guild_list.cpp`, `guild_info.cpp`, `guild_members.cpp`, `profile.cpp`, `people_nav.cpp`, `notifications.cpp`, `link_view.cpp` if only features use it, `src/app/feed_worker.cpp`, `src/app/people_worker.cpp`, `src/feed/*` as applicable
- Modify: `platformio.ini` (`cardputer_adv` gets `-DRC_FEAT_CMAIL=0 -DRC_FEAT_FEED=0 -DRC_FEAT_GUILDS=0 -DRC_FEAT_PEOPLE=0 -DRC_FEAT_UNREAD=0`)

**Interfaces:**
- Consumes: `features.h` (Task 1). `router::Id` keeps every value.

Rooms (`room_list.cpp`, `room.cpp` as a room, not `RoomScreen{true}` the conversation) stay. `RoomScreen{true}` (`Id::Conversation`) is C-Mail and goes with it. Mentions in rooms still beep (speaker kept); the Unread screen is gone.

- [ ] **Step 1: Set the five flags in the env; run** `nice pio run -e cardputer_adv`. Expected: compile/link errors listing every wiring point.
- [ ] **Step 2: Guard each wiring point and wrap each feature file** until it builds.
- [ ] **Step 3: Check the reachability**: `grep -n "router::go(router::Id::\(CmailList\|Conversation\|FeedList\|Entry\|Composer\|Watched\|Bookmarks\|GuildList\|Guild\|Members\|Profile\|UserPosts\|Notifications\)" src -r` — every hit sits under the matching `#if RC_FEAT_X`. Record the result in the report.
- [ ] **Step 4: Build** `nice pio run -e cardputer_adv` and `nice pio run -e m9`; run `nice pio test -e native`. Expected: SUCCESS, SUCCESS, all pass. Report cardputer flash/RAM vs Task 4.
- [ ] **Step 5: Commit** `git commit -m "Compile out C-Mail, Feed, Guilds, People and Unread"`

### Task 6: Compile out spell check, emoji, OTA and mascot

**Files:**
- Modify: `tools/gen_spell_dict.py` (write an empty dictionary header when the env's CPPDEFINES has `RC_FEAT_SPELL=0`), `src/spell_en.cpp`, `src/screens/room.cpp` (spell strip, emoji tray key), `src/app/ui_task.cpp` (`symbols` emoji tray, OTA hooks), `src/ui/renderer.cpp` (`emoji_cells.h` include), `src/main.cpp` (`ota::start`, mascot), `src/app/ota_service.cpp` (wrap), `src/store/mascot_store.cpp` (wrap), `src/web/web_server.cpp` and `src/web/page.h` (`/api/mascot`, Mascot section, update items), `src/screens/settings.cpp` (update items), `src/screens/splash.cpp` and the screensaver (built-in Fluffy when mascot is off), `tools/gen_mascot_presets.py` if it fails without the partition
- Modify: `platformio.ini` (`cardputer_adv` adds `-DRC_FEAT_SPELL=0 -DRC_FEAT_EMOJI=0 -DRC_FEAT_OTA=0 -DRC_FEAT_MASCOT=0`)

**Interfaces:**
- Consumes: `emojiPlaceholder()` (Task 3).

With emoji off the symbol table (non-emoji symbols) stays; only the emoji tray and its bitmaps go.

- [ ] **Step 1: Set the four flags; run** `nice pio run -e cardputer_adv`. Expected: errors at the wiring points.
- [ ] **Step 2: Guard and wrap** until it builds; confirm `emoji_cells.h` and the spell dictionary are absent from the cardputer image (`nice pio run -e cardputer_adv -v` link map or `xtensa-esp32s3-elf-nm` on `firmware.elf` shows no `kEmojiCells`/dictionary symbols).
- [ ] **Step 3: Build** `nice pio run -e cardputer_adv`, `nice pio run -e tdeck`, `nice pio run -e x4`; run `nice pio test -e native`. Expected: SUCCESS ×3, all pass. Report cardputer flash/RAM.
- [ ] **Step 4: Commit** `git commit -m "Compile out spell check, emoji, OTA and mascot"`

### Task 7: Scripts, release, flasher and docs

**Files:**
- Modify: `scripts/build-upload-monitor.sh` (flag `-C` → `cardputer_adv`; `-B` includes it; `--help`), `scripts/release.sh` (`RELEASE_ENVS` gains `cardputer_adv`; a `NO_OTA_ENVS=(cardputer_adv)` list whose envs skip `-ota.bin`, `.sig` and their checks), `.github/workflows/release.yml` and `.github/workflows/build.yml` if they list envs, `site/index.html` (`LABELS` gains `"cardputer-adv": "M5Stack Cardputer ADV"`; hide Make your Fluffy and Phrases when it is the picked board, and flash without the mascot blob), `scripts/pages-site.sh` if it assumes an OTA file per board
- Modify docs: `AGENTS.md` (board list, commands block `pio run -e cardputer_adv`, `-C`, Layout paragraph for `src/hal/cardputer_adv/`, the feature flags in one line, nine→ten envs), `docs/BUILD.md` (env, 8 MB layout, no OTA for it, feature flags), `docs/USE.md` (Cardputer keys: Fn arrows, Esc Back, Fn+Backspace Home), `docs/TESTING.md` ("Open hardware checks": panel MADCTL/offsets, every key, backlight, battery, beep, SD backup, sign-in free heap, room read/send)
- Test: `site/*.test.mjs` if the hide logic is testable there

- [ ] **Step 1: Update the scripts and release**; run `bash -n scripts/release.sh scripts/build-upload-monitor.sh` and `scripts/release.sh --dist-only --only cardputer_adv --no-clean`. Expected: `dist/` has `monster-cyberspace-cardputer-adv-v*.bin` and no `-ota.bin`; the script reports OK.
- [ ] **Step 2: Update the site**; run `node --test site/*.test.mjs`. Expected: all pass.
- [ ] **Step 3: Update the docs.**
- [ ] **Step 4: Commit** `git commit -m "Cardputer ADV in scripts, release, flasher and docs"`
