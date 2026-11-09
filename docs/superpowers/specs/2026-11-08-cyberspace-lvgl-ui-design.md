# LVGL UI rewrite: design

Date: 2026-11-08. Branch: `lvgl-ui`. Status: approved design, awaiting plan.

## 1. Goal

Rewrite the device UI on LVGL so it uses LVGL's widgets, styling and input
handling instead of our cell-grid renderer and hand-made widgets, while
keeping:

- the **same UX flow**: every screen, the order you move between them, Back,
  overlays, menus, key bindings, touch behaviour and wake rules;
- the **same themes**: all 19 (`ThemeId`, saved by index, same colours);
- the **retro terminal look**: monospace Terminus text in the same five
  sizes, Noto Emoji for wide emoji.

Success: every firmware env except the X4 builds with the LVGL UI, the M9
(bring-up target) builds clean, and a human hardware check on the M9 finds the
same flow and themes as `main`. The agent builds `m9` but never flashes.

### Out of scope

- The Xteink X4 (`x4`): it keeps today's grid UI (too little RAM for LVGL).
- Any change to the UX, layout intent, themes or text; new animations beyond
  LVGL defaults (scrolling is fine, transitions off).
- Network, API, workers, storage, web config, OTA, the flasher site.

## 2. Approach

A single native rewrite (option B of the brainstorm): every screen is rebuilt
as an LVGL object tree in one change on the `lvgl-ui` branch. No mixed
grid/LVGL router. The X4 keeps a frozen copy of the grid path.

## 3. Architecture

### 3.1 Library and config

- LVGL **9.5.0** (`lvgl/lvgl@9.5.0`, as camillia-mt), in the common
  `lib_deps` of every env except `x4`.
- `lib/lvui/conf/lv_conf.h` (`LV_CONF_INCLUDE_SIMPLE`): RGB565 colour depth,
  custom allocator in PSRAM (`heap_caps_malloc(MALLOC_CAP_SPIRAM)`), only the
  widgets this spec lists enabled, no built-in fonts beyond what's needed,
  `LV_USE_OS LV_OS_NONE`, logging off in release.
- LVGL runs only on `ui_task`'s thread. Nothing else calls `lv_*`.

### 3.2 Display

- `lib/lvui/src/lvui/display.{h,cpp}` makes one `lv_display_t` per board, sized
  `board::panelWidth()` x `board::panelHeight()`, partial render mode with two
  draw buffers (PSRAM; 1/10 of the panel each), flushing through
  `board::pushRows`. Rotation and the 2x boards (T-Display P4, Tanmatsu)
  keep working because they live behind `pushRows`/`panelWidth`.
  `lv_display_set_resolution` follows a rotation change.
- `board::pushRows` takes a full-width row band today. The flush callback
  copies the dirty area into a full-width band buffer (or LVGL is set to
  full-width invalidation) so no HAL changes.
- **E-paper** (T-Deck Pro): LVGL renders RGB565; the flush packs to 1 bit as
  today's path does, and `ui/eink` still decides partial vs full refresh.
  `paperTheme()` styles apply (outlined selections).
- Screenshots (`scripts/screenshot.py`, `ui/screenshot`) read a snapshot
  (`lv_snapshot_take`) instead of the renderer frame; the serial protocol is
  unchanged.

### 3.3 Fonts

- No font conversion. A custom `lv_font_t` adapter (`get_glyph_dsc` /
  `get_glyph_bitmap` callbacks, 1 bpp) reads the existing Terminus cell
  fonts (`src/ui/fonts/cell*.h`, `CellFont`, `glyphFor` with its aliases)
  and the wide Noto Emoji halves (`emoji_cells.h`), so there is no second
  copy of the glyphs in flash. Bold is drawn as today (one-pixel smear).
- The text-size setting picks the font; `board::fixedTextSize` still wins.
  Everything is monospace, so column-based layouts still line up.

### 3.4 Themes

- `lib/lvui/src/lvui/styles.{h,cpp}`: from a `Theme`, build a fixed set of
  `lv_style_t`s: screen, top bar, hint bar, list item, focused, selected,
  input, cursor, drawer, drawer edge, error, mention, dim, time, sys, nick,
  self nick. `colorNicks`, `boldNicks`, `outlined` and `nickLightness` keep
  their meaning (`outlined`: border instead of background fill).
- Changing theme rebuilds the styles and calls `lv_obj_report_style_change`.
- `ui/theme.{h,cpp}` (the colour tables) is unchanged.

### 3.5 Screens and router

- `Screen` loses `render(Grid&, ...)` and gains:
  - `build(lv_obj_t* root)`: creates the screen's objects under `root`;
  - `update(int64_t nowMs)`: per-frame changes (clock, cursor, typing, timers);
  - `onEvent` updates objects in place.
  Every other virtual stays (`enter`, `leave`, `onKey`, `acceptsText`,
  `consumesBackspace`, `hasOverlay`, `closeOverlays`, `paletteLabel`,
  `compactText`).
- The router keeps `router::Id` (Splash, Setup, Home, RoomList, Room,
  ComingSoon, LoginForm, CmailList, Conversation, FeedList, Entry, Composer,
  Watched, Bookmarks, Settings, Wifi, GuildList, Guild, Members, Profile,
  UserPosts, Notifications, Topics, Topic, Following), `go`, `goFrom`, Back
  and `backToOrigin`. On `go` it deletes the old screen's `lv_obj` tree and
  calls `build` on the new one.
- `onKey` stays the place screens handle keys, so key behaviour is ported
  logic, not re-invented: the keypad indev delivers keys to the focused
  object, and screens move focus with `lv_group_focus_next/prev` or their own
  rules as today.

### 3.6 Input

- `lib/lvui/src/lvui/indev.{h,cpp}`:
  - Keypad indev fed from `board::pollKey` (portable keys, keymaps in
    `lib/core/src/input/` unchanged), one `lv_group_t` per screen.
  - Pointer indev fed from `board::readTouch` on touch boards. It replaces
    `ui/tap_map`, `input/gestures` and `input/touch_resolve`; long press,
    swipe-to-go-back and scroll map to LVGL events with the same results.
  - Global keys (Home, Back closing overlays first, ScreenOff, wake, F10
    help) stay in `ui_task` before keys reach LVGL.
- The on-screen keyboard is `lv_keyboard` with our layouts (`Osk` maps),
  docked at the bottom when `acceptsText()`; `board::textEntry` (emoji tray
  key) unchanged.

## 4. Components

New shared views live in `lib/lvui/src/lvui/views/`. Mapping from today's:

| Today (`lib/core/src/ui/widgets/`) | LVGL |
|---|---|
| `status_bar`, `banner` | `TopBar`: `lv_obj` row of labels (time, Wi-Fi, battery, unread, notify) and a banner label |
| `list_view`, `entry_list` | Flex column of focusable `lv_button`s in a scrolling `lv_obj` |
| `side_nav` | Three-button strip (▲ ● ▼), T-Deck Pro |
| `log_view`, `text_view`, `text_area` | `LogView`: scrolling `lv_obj`; one `lv_spangroup` per visible message (nick colour from `nick_color`); virtualised, only messages near the viewport have objects |
| `input_line` | One-line `lv_textarea` with our cursor style |
| `spell_strip`, `symbol_table`, `palette`, emoji tray | `lv_buttonmatrix` rows |
| `osk` | `lv_keyboard` with custom maps |
| `overlay`, `canned_picker` | `MenuOverlay`: modal on `lv_layer_top()` with a list |
| `drawer` | Side panel on `lv_layer_top()` |
| `slider`, `scroll_bar` | `lv_slider`, LVGL scrollbars styled per theme |
| `button_grid`, `home_tiles`, `button_pad` | Grid-layout `lv_button`s (tiles on touch boards, rows on keyed ones, `homeSpanFirst` kept) |
| `qr_cells` | `lv_qrcode` |
| splash / mascot art | Labels in the big font; screensaver its own LVGL screen |

Portable logic stays in `lib/core` with its tests: `wrap`, `text_width`,
`room_layout`/`feed_layout`/`guild_layout`/`profile_layout`/`splash_layout`
as data (what lines and spans to show), `home_items`, `unread_inbox`,
`nick_color`, `timefmt`, `theme`, `eink`, `rotation`, `scale2x`,
`screen_timeout`, `notify_light`, `haptics`, `brightness`, input keymaps.

The `lib/core` widgets above hold both state (selection, scroll, text,
cursor) and Grid drawing. The LVGL views bind to that state, so key handling
and its native tests are kept; their Grid drawing stays in `lib/core` for the
X4 and is unused (dropped by the linker) elsewhere. The LVGL views live in a
new portable library, `lib/lvui/` (LVGL + `lib/core`, no Arduino), so the
host can test them; board glue is in `src/ui/lv/`.

Removed from LVGL builds: `src/ui/renderer`, `ui/tap_map` and
`input/touch_resolve` (LVGL resolves taps; a tap on a view emits the same
`KeyEvent` today's tap map does). `input/gestures` stays for edge swipes
(Back, Options).

### X4

The X4 env builds a frozen copy of today's `src/screens/`, `src/ui/` and
`src/app/ui_task.cpp` under `src/grid_ui/` (its `build_src_filter` swaps
them in; `-Isrc/grid_ui` comes before `-Isrc`). The `lib/core` widgets are
shared, not copied. No new features go there.

## 5. Data flow

Each `ui_task` loop:

1. Drain worker/net events → `screen->onEvent` (updates objects in place).
2. Global keys; then `board::pollKey` → keypad indev; touch → pointer indev.
3. `screen->update(now)`.
4. `lv_timer_handler()`: LVGL redraws only invalidated areas (replaces the
   Grid's dirty rows) and flushes through `pushRows`.

Workers never call LVGL; their queues are unchanged. Every write the
firmware makes still comes from a user's key press, from the same handlers
(LVGL event callbacks call the existing command functions); no automatic
retries.

## 6. Memory and failure handling

- PSRAM boards: LVGL heap and draw buffers in PSRAM.
- Cardputer ADV (no PSRAM): `RC_LV_HEAP` internal heap (about 48 KB) and a
  240x20 draw buffer. If TLS no longer fits, it drops to Compact-only text
  and fewer live objects; it never assumes PSRAM. There (`RC_LV_LEAN`) the
  room log and lists are painted: one object draws every row itself, with
  the same pixels and taps, so each of ESP-IDF's poisoned heap blocks
  (+12 B) isn't paid per row. The `RC_LV_HEAP` budget counts that overhead.
- If LVGL can't allocate a screen, the router shows a plain "Out of memory"
  label and stays on the previous `Place`.
- Room logs and feeds are virtualised; `RC_CHAT_CAP` / `RC_MAX_REPLIES`
  caches unchanged.
- Screen off stops calling `lv_timer_handler`; the 10 s screensaver and the
  wake rules (`anyInputWakes`, hold centre) are unchanged.

## 7. Testing

- `pio test -e native`: portable `lib/core` tests stay green; removed
  widgets take their tests with them.
- New `native_lv` env: LVGL built for the host with a memory display (no
  SDL), testing `lib/lvui`: fonts, styles for all 19 themes, the display
  flush, the screen host, and every view (it mirrors its model, and a tap
  emits the same `KeyEvent` as today's tap map). Screens' `onKey` logic is
  unchanged; screens themselves depend on the app and Arduino, so they are
  checked by the `m9` build and the hardware check, not on the host.
- `pio run -e m9` must build clean first; then
  `scripts/build-upload-monitor.sh -B` for all thirteen envs (X4 on the grid
  path). Report RAM/flash against `main`.
- The agent never flashes. `docs/TESTING.md` gets an "LVGL UI" hardware
  check per board, M9 first.
- Docs: `AGENTS.md`, `docs/BUILD.md`, `docs/USE.md` updated.

## 8. Risks

- **Size of the change:** about 42k lines of UI rewritten at once; nothing on
  the M9 is usable until all screens are done. Mitigated by the `native_lv`
  flow tests and building `m9` after each task.
- **Cardputer RAM:** may not fit alongside TLS; measured during the plan.
- **E-paper refresh:** LVGL invalidation must feed `ui/eink` well enough to
  avoid full refreshes on every change.
- **Font size:** LVGL fonts for every Terminus glyph at 5 sizes plus bold
  may cost more flash than the cell fonts; measured, glyph set trimmed only
  if a board overflows.
