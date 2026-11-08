# LVGL UI Rewrite Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the cell-grid renderer with an LVGL 9.5.0 UI on every board but the X4, with the same UX flow and the same 19 themes.

**Architecture:** A portable library `lib/lvui/` (LVGL + `lib/core`, no Arduino, host-testable in a new `native_lv` env) holds the font adapter, theme styles, display flush, screen host and views. Views bind to the existing `lib/core` widget state (ListView, LogView, InputLine, ...) and turn taps into the same `KeyEvent`s the tap map makes today, so every screen's `onKey` is unchanged. Screens gain `build(lv_obj_t*)`/`update(nowMs)`; `ui_task` switches to LVGL in one task once all screens are ported. The X4 keeps a frozen grid copy in `src/grid_ui/`.

**Tech Stack:** C++17, PlatformIO, Arduino-ESP32 3.3.12 (pioarduino), LVGL 9.5.0, Unity.

**Spec:** `docs/superpowers/specs/2026-11-08-cyberspace-lvgl-ui-design.md`

## Global Constraints

- LVGL `lvgl/lvgl@9.5.0`; config `lib/lvui/conf/lv_conf.h` via `-DLV_CONF_INCLUDE_SIMPLE -Ilib/lvui/conf`; RGB565; `LV_USE_OS LV_OS_NONE`; LVGL called only from `ui_task`'s thread.
- Same UX flow: `router::Id` set, `go`/`goFrom`/Back/`backToOrigin`, overlays, key bindings, wake rules unchanged. Screens' `onKey` bodies are not rewritten.
- Same themes: `ui/theme.{h,cpp}` untouched; 19 themes, saved by index.
- Retro look: monospace Terminus cell fonts and Noto Emoji halves only, via the `lv_font_t` adapter. No other fonts.
- No HAL changes outside `src/hal/` needs; hardware only through `src/hal/board.h`; `lib/lvui` never includes `board.h` or Arduino.
- X4 (`x4`) not on LVGL; it builds `src/grid_ui/`.
- Hard rules from `AGENTS.md`: never call the live API; every write from a key press, no retries; never log passwords or tokens.
- The agent builds (`pio run -e m9` after every task) but never flashes. Commit after each task on `lvgl-ui`; never push.
- Each task: `pio test -e native` and `pio test -e native_lv` (from Task 2) green, `pio run -e m9` builds.

## Review Focus

1. Changing theme with a screen open: every object restyles at once, nothing keeps old colours → Task 4 test `test_restyle_on_theme_change`.
2. Rotation (touch boards) or text-size change with a screen open: resolution and font follow, screen rebuilt, no stale layout → Task 5 test `test_resize_rebuilds`, Task 3 test `test_font_for_size_fixed_wins`.
3. A full room log (`RC_CHAT_CAP` messages, long lines, wide emoji): scrolling keeps the object count bounded → Task 7 test `test_log_view_virtualised`.
4. LVGL runs out of memory building a screen: stay on the previous screen with "Out of memory" → Task 5 test `test_host_build_failure_keeps_previous`.
5. A tap on a row while a menu overlay is open hits the overlay, not the list beneath → Task 6 test `test_overlay_captures_taps`.

---

### Task 1: Freeze the X4 grid UI

**Files:**
- Create: `src/grid_ui/` (copies of `src/screens/`, `src/ui/renderer.{h,cpp}`, `src/ui/fonts/` stays shared, `src/app/ui_task.cpp` → `src/grid_ui/app/ui_task.cpp`)
- Modify: `platformio.ini` `[env:x4]` (`build_src_filter = +<*> -<screens/> -<ui/renderer.cpp> -<app/ui_task.cpp> -<ui/lv/>`, `build_flags` add `-Isrc/grid_ui` before others, `-DRC_UI_GRID=1`)

**Interfaces:**
- Produces: `RC_UI_GRID` (1 only on `x4`); every later task may change `src/screens/`, `src/ui/`, `src/app/ui_task.cpp` without touching the X4.

- [x] **Step 1:** Copy the files; in the copies change nothing but include paths if any break.
- [x] **Step 2:** `pio run -e x4` → SUCCESS, RAM/flash identical (±1 KB) to `main`.
- [x] **Step 3:** `pio run -e m9` → SUCCESS (unaffected).
- [x] **Step 4:** Commit `X4: build a frozen copy of the grid UI`.

### Task 2: LVGL and the `native_lv` env

**Files:**
- Create: `lib/lvui/library.json` (deps: `lvgl/lvgl@9.5.0`, core), `lib/lvui/conf/lv_conf.h`, `lib/lvui/src/lvui/lv_init.{h,cpp}`, `test/test_lv_smoke/test_main.cpp`
- Modify: `platformio.ini`: `[env]` stays; each LVGL env (all firmware envs but `x4`) adds `lvgl/lvgl@9.5.0` and `-DLV_CONF_INCLUDE_SIMPLE -Ilib/lvui/conf`; new `[env:native_lv]` (like `native`, plus LVGL, `test_filter = test_lv_*`); `[env:native]` `test_ignore` adds `test_lv_*`; `x4` gets `lib_ignore = lvui`.

**Interfaces:**
- Produces: `namespace lvui { void init(); }` (calls `lv_init`, sets tick from a callback); `void setTickSource(uint32_t (*ms)());`.
- `lv_conf.h`: under `RC_NATIVE`, `LV_STDLIB_CLIB`; on ESP32, `LV_STDLIB_CUSTOM` with `lv_malloc_core` etc. in `lv_init.cpp` using `heap_caps_malloc(…, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)` when `BOARD_HAS_PSRAM`, else internal heap capped at `RC_LV_HEAP` (default 49152).

- [x] **Step 1:** Test `test_lv_smoke`: `lvui::init()`; create a 64x32 display with a dummy flush; `lv_label_create`; `lv_timer_handler()`; assert flush was called at least once.
- [x] **Step 2:** `pio test -e native_lv` → FAIL (no lib).
- [x] **Step 3:** Implement.
- [x] **Step 4:** `pio test -e native_lv` → PASS; `pio test -e native` → 759/759; `pio run -e m9` → SUCCESS.
- [x] **Step 5:** Commit `LVGL 9.5.0 and the native_lv test env`.

### Task 3: Cell-font adapter

**Files:**
- Create: `lib/lvui/src/lvui/cell_font.{h,cpp}`, `test/test_lv_font/test_main.cpp`
- Modify: `native_lv` `build_flags` add `-Isrc/ui/fonts`.

**Interfaces:**
- Consumes: `rc::CellFont`, `rc::glyphFor`, `emoji_cells.h` halves, `rc::TextSize`, `ui/text_width.h`.
- Produces: `const lv_font_t* lvui::font(rc::TextSize s, bool big = false, bool bold = false);` (`big` = the next size up, as header rows; `bold` = each row ORed with itself 1 px right, ruling R4), `const lv_font_t* lvui::fontFor(rc::TextSize saved, std::optional<rc::TextSize> fixed);` (fixed wins). Fonts: Compact 6x12, Comfortable 8x16, Large 10x18; big: 8x14/10x18/12x24 per today's `bigCellSize`. Glyph advance = cell width; wide emoji = 2 cells; `line_height` = cell height; `base_line` = 0.

- [x] **Step 1:** Tests: `test_ascii_glyph_matches_cell` (bitmap of 'A' via `lv_font_get_glyph_bitmap` equals `glyphFor(cell8x16,'A')` pixels); `test_missing_uses_alias`; `test_emoji_two_cells` (advance 16 at 8x16); `test_font_for_size_fixed_wins`.
- [x] **Step 2:** Run → FAIL.
- [x] **Step 3:** Implement the `get_glyph_dsc`/`get_glyph_bitmap` callbacks over `CellFont` (1 bpp, `LV_FONT_GLYPH_FORMAT_A1`).
- [x] **Step 4:** Run → PASS; `pio run -e m9` → SUCCESS.
- [x] **Step 5:** Commit `LVGL: fonts read from the Terminus cells`.

### Task 4: Theme styles

**Files:**
- Create: `lib/lvui/src/lvui/styles.{h,cpp}`, `test/test_lv_styles/test_main.cpp`

**Interfaces:**
- Produces: `enum class lvui::Role { Screen, Bar, Hint, Item, Focused, Selected, Input, Cursor, Drawer, DrawerEdge, Error, Mention, Dim, Time, Sys, Nick, SelfNick };` `void lvui::applyTheme(const rc::Theme&);` `lv_style_t* lvui::style(Role);` `void lvui::addStyle(lv_obj_t*, Role, lv_style_selector_t sel = 0);` `const rc::Theme& lvui::currentTheme();`. `outlined` → Focused/Selected use 1 px border in `selectBg`, no fill. `applyTheme` rewrites the static styles and calls `lv_obj_report_style_change(nullptr)`.

- [x] **Step 1:** Tests: for each of the 19 `themeFromIndex(i)`, `applyTheme` then Screen bg == `t.bg`, Item text == `t.fg`, Selected bg == `t.selectBg` (or border when `outlined`); `test_restyle_on_theme_change` (a label styled `Item` under Default, after `applyTheme(theme(C64))` and `lv_timer_handler`, has `lv_obj_get_style_text_color` == C64 `fg`).
- [x] **Step 2:** Run → FAIL. **Step 3:** Implement. **Step 4:** Run → PASS; `pio run -e m9`.
- [x] **Step 5:** Commit `LVGL: styles from the 19 themes`.

### Task 5: Display, input bridge and screen host

**Files:**
- Create: `lib/lvui/src/lvui/display.{h,cpp}`, `lib/lvui/src/lvui/input.{h,cpp}`, `lib/lvui/src/lvui/host.{h,cpp}`, `test/test_lv_display/test_main.cpp`, `test/test_lv_host/test_main.cpp`
- Create: `src/ui/lv/board_glue.{h,cpp}` (wires `board::pushRows`, `panelWidth/Height`, `readTouch`, `eink()`; not built yet by `ui_task`)

**Interfaces:**
- Produces:
  - `struct lvui::DisplayConfig { int w, h; void (*pushRows)(const uint16_t* frame, int y0, int y1); bool mono; };` `lv_display_t* lvui::displayBegin(const DisplayConfig&);` `void lvui::displayResize(int w, int h);` `const uint16_t* lvui::frame();`. Keeps one full RGB565 frame (PSRAM; strip of `w*20` on non-PSRAM), LVGL renders partial into it, flush calls `pushRows(frame, y1, y2+1)` for the dirty band. `mono`: pixels thresholded per `rgb565Dark` before the HAL packs (same as today).
  - `void lvui::pointerBegin(bool (*read)(int16_t&, int16_t&));` `void lvui::setKeySink(void (*sink)(const rc::KeyEvent&));` `void lvui::emit(const rc::KeyEvent&);` (views call this on tap).
  - `class lvui::ScreenHost { public: bool show(const std::function<void(lv_obj_t*)>& build); lv_obj_t* root(); void rebuild(); };` `show` builds into a new `lv_obj` and on success deletes the old one; if allocation fails (`lv_obj_create` null or `lv_mem_monitor` free below `kMinFree = 8192` after build) deletes the new tree, keeps the old, shows a top-layer label "Out of memory" for 3 s and returns false.
- [x] **Step 1:** Tests: `test_flush_pushes_dirty_band` (a label at y 20..36 changes → `pushRows` called with a band covering 20..36 and not 0); `test_resize_rebuilds` (`displayResize(32,64)` → `lv_display_get_horizontal_resolution` 32, host `rebuild` re-runs the build fn); `test_host_build_failure_keeps_previous` (build fn that sets a failure flag → returns false, old root still child of screen, "Out of memory" label exists); `test_emit_reaches_sink`.
- [x] **Step 2:** Run → FAIL. **Step 3:** Implement. **Step 4:** Run → PASS; `pio run -e m9`.
- [x] **Step 5:** Commit `LVGL: display flush, input bridge, screen host`.

### Task 6: Views — bars, lists, overlays

**Files:**
- Create in `lib/lvui/src/lvui/views/`: `top_bar`, `banner`, `list_box`, `entry_list_box`, `menu_overlay`, `drawer_panel`, `side_nav_strip` (`.h/.cpp` each); `test/test_lv_views_lists/test_main.cpp`

**Interfaces:**
- Every view: `void create(lv_obj_t* parent);` `void sync(const Model&);` (updates objects in place; no rebuild if unchanged) and, when tappable, emits on `LV_EVENT_CLICKED` the `KeyEvent` today's `TapMap` gives for that place: rows → `{Activate, index=row, target=ListId, then=Select}`, as built in `ui/tap_map.cpp`/`input/touch_resolve.cpp` (read them; copy the mapping exactly).
- Models: `TopBar` ← status bar data (`ui/widgets/status_bar.h` input struct); `Banner` ← `ui/widgets/banner.h`; `ListBox` ← `rc::ListView` (+ `ListId`); `EntryListBox` ← `rc::EntryList`; `MenuOverlay` ← `rc::Overlay` (on `lv_layer_top()`, modal: `LV_OBJ_FLAG_CLICKABLE` backdrop); `DrawerPanel` ← `rc::Drawer`; `SideNavStrip` ← `side_nav` (emits Up / Select / Down).
- Selected row scrolled into view (`lv_obj_scroll_to_view`, no animation).
- [x] **Step 1:** Tests: `test_list_box_mirrors_model` (3 items, sel 1 → 3 children, child 1 has `LV_STATE_CHECKED` styled Selected, right text shown); `test_list_tap_emits_activate`; `test_overlay_captures_taps` (tap point inside overlay row 0 over list row 2 → one emit, target Menu, index 0); `test_top_bar_texts`; `test_side_nav_emits`.
- [x] **Step 2:** FAIL. **Step 3:** Implement. **Step 4:** PASS; `pio run -e m9`. **Step 5:** Commit `LVGL views: bars, lists, overlays`.

### Task 7: Views — text

**Files:**
- Create in `lib/lvui/src/lvui/views/`: `log_box`, `text_box`, `input_field`, `slider_box`; `test/test_lv_views_text/test_main.cpp`

**Interfaces:**
- `LogBox` ← `rc::LogView` + the room layout lines (`ui/room_layout`): one row object per *visible* laid-out line plus 1 screen above/below; nick spans coloured by `nick_color` (or `Nick`/`SelfNick` per `colorNicks`), bold per `boldNicks`; mention lines styled Mention; scrolling moves the model's offset (keys) or emits `Up/Down` per line (touch drag). Tap on a line → same KeyEvent as tap map (`ListId::Log`).
- `TextBox` ← `rc::TextView` / `rc::TextArea` (wrapped by `ui/wrap`, read-only).
- `InputField` ← `rc::InputLine`: one-line `lv_textarea`-look built from a label + Cursor-styled cell (cursor shown per `cursorShown()`), so text editing stays in `InputLine`; tap → `{Focus, index=text pos, target=field id}`.
- `SliderBox` ← `rc::Slider`.
- [x] **Step 1:** Tests: `test_log_view_virtualised` (300 messages of 3 lines; children count ≤ 3 × visible rows; after scrolling to top, first row text is message 0); `test_log_nick_colour`; `test_input_field_cursor_and_focus_tap`; `test_text_box_wraps_like_wrap`.
- [x] **Step 2:** FAIL. **Step 3:** Implement. **Step 4:** PASS; `pio run -e m9`. **Step 5:** Commit `LVGL views: log, text, input, slider`.

### Task 8: Views — keypads and tiles

**Files:**
- Create in `lib/lvui/src/lvui/views/`: `button_rows` (spell strip, symbol table, palette, emoji tray, canned picker), `keyboard` (Osk), `tile_grid` (button_grid, home_tiles, button_pad), `qr_box`; `test/test_lv_views_keys/test_main.cpp`

**Interfaces:**
- `ButtonRows` ← each model (`SpellStrip` emits `{Spell, index}`, `SymbolTable` emits `{Char, ch}`, palette/emoji/canned as their tap map entries).
- `Keyboard` ← `rc::Osk`: `lv_buttonmatrix` with Osk's rows and layer; emits Osk's KeyEvents; height = today's `Osk::gridRows()` × cell height.
- `TileGrid` ← `home_tiles` / `button_grid` / `button_pad` (respects `homeSpanFirst`).
- `QrBox` ← `ui/qr_cells` (`lv_qrcode` off; draws modules from `qr_cells` so output matches).
- [x] **Step 1:** Tests: `test_osk_layer_and_emit`; `test_symbols_emit_char`; `test_home_tiles_span_first`; `test_spell_emit`.
- [x] **Step 2:** FAIL. **Step 3:** Implement. **Step 4:** PASS; `pio run -e m9`. **Step 5:** Commit `LVGL views: keyboards, tiles, QR`.

### Task 9: Screen interface and the first screens

**Files:**
- Modify: `src/screens/router.h` (`Screen` gains `virtual void build(lv_obj_t* root) {}` and `virtual void update(int64_t nowMs) {}`; `render` stays until Task 14), `src/screens/screens.h`
- Modify: `splash.cpp`, `setup.cpp`, `login_form.cpp`, `wifi.cpp`, `coming_soon.cpp`, `home.cpp`, `backup_picker.{h,cpp}`, `button_pad.{h,cpp}`

**Interfaces:**
- Consumes: Tasks 3–8. Pattern every screen follows: views as members; `build` creates them and calls `sync`; `onEvent`/`onKey` keep their bodies and end with `sync` of changed views; `update` does clock/timer work that `render` did with `nowMs`.
- [x] **Step 1:** Port each screen: `build`/`update` reproduce what `render` draws (same rows, labels, hints, positions in cells × font size).
- [x] **Step 2:** `pio run -e m9` → SUCCESS (still drawn by the grid).
- [x] **Step 3:** Commit `LVGL: splash, setup, login, Wi-Fi, home`.

### Task 10: Rooms, C-Mail and Unread

**Files:** `room.{h,cpp}` (Room and Conversation), `room_list.cpp`, `cmail_list.cpp`, `notifications.cpp`, `people_nav.{h,cpp}`
- [x] **Step 1:** Port as Task 9 (LogBox, InputField, spell strip, typing line).
- [x] **Step 2:** `pio run -e m9` → SUCCESS. **Step 3:** Commit `LVGL: rooms, C-Mail, Unread`.

### Task 11: Feed

**Files:** `feed_list.cpp` (Feed, Watched, Bookmarks, Topic, Following, UserPosts), `topics.cpp`, `entry.{h,cpp}`, `composer.{h,cpp}`, `link_view.{h,cpp}`
- [ ] **Step 1:** Port. **Step 2:** `pio run -e m9`. **Step 3:** Commit `LVGL: Feed, entries, composer`.

### Task 12: Guilds and People

**Files:** `guild_list.cpp`, `guild_info.{h,cpp}`, `guild_members.cpp`, `profile.cpp`
- [ ] **Step 1:** Port. **Step 2:** `pio run -e m9`. **Step 3:** Commit `LVGL: Guilds, profiles`.

### Task 13: Settings

**Files:** `settings.cpp`
- [ ] **Step 1:** Port (theme picker previews via `applyTheme`, text size via `fontFor`, rotation, sliders).
- [ ] **Step 2:** `pio run -e m9`. **Step 3:** Commit `LVGL: Settings`.

### Task 14: Switch `ui_task` to LVGL

**Files:**
- Modify: `src/app/ui_task.cpp`, `src/screens/router.{h,cpp}` (router owns a `lvui::ScreenHost`; `go` calls `host.show([&](lv_obj_t* r){ s.build(r); })` and, on false, keeps the current `Place`), `src/screens/router.h` (remove `render`), every screen (delete `render`), `src/screens/draw.h` (delete or reduce to non-Grid helpers), `src/ui/lv/board_glue.cpp`, `src/main.cpp` if it touches the renderer
- Delete (LVGL builds): `src/ui/renderer.{h,cpp}`; `ui_task` stops using `TapMap`/`touch_resolve`; `gestures` still feeds edge swipes.

**Interfaces:**
- Loop per spec §5: events → `onEvent`; global keys; `pollKey` → `screen().onKey`; `lvui` pointer indev; `screen().update(now)`; `lv_timer_handler()`; screen off → skip `lv_timer_handler`.
- Global layers on `lv_layer_top()`: symbol table, canned picker, top banner, OSK docked when `acceptsText()` (screen root shrinks by the keyboard height), F10 keys help, screensaver. Theme change → `applyTheme`; text size or rotation → `displayResize` + `host.rebuild()`. Screenshot command reads `lvui::frame()`.
- [ ] **Step 1:** Implement.
- [ ] **Step 2:** `pio run -e m9` → SUCCESS; record RAM/flash vs `main` in the commit message.
- [ ] **Step 3:** `pio test -e native` and `pio test -e native_lv` → PASS.
- [ ] **Step 4:** Commit `LVGL: ui_task draws with LVGL`.

### Task 15: Every other board

**Files:** `platformio.ini`, per-board flags only where needed (`RC_LV_HEAP` on `cardputer_adv`), `src/ui/lv/board_glue.cpp` (e-paper: `board::eink()` → `mono`, invalidation feeds `ui/eink` as today's dirty rows did)
- [ ] **Step 1:** `scripts/build-upload-monitor.sh -B` → all thirteen envs SUCCESS (X4 on `src/grid_ui`).
- [ ] **Step 2:** Cardputer: if RAM leaves less than today's TLS headroom (61,368 B static today), lower `RC_LV_HEAP`/draw strip until it does; record the numbers.
- [ ] **Step 3:** Commit `LVGL: every board but the X4`.

### Task 16: Docs

**Files:** `AGENTS.md` (layout: `lib/lvui`, `src/ui/lv`, `src/grid_ui`; commands: `pio test -e native_lv`), `docs/BUILD.md`, `docs/USE.md` (only if anything visible differs), `docs/TESTING.md` ("LVGL UI" open hardware check per board, M9 first: every screen reached, Back from each, each theme, text sizes, touch where present)
- [ ] **Step 1:** Edit. **Step 2:** Commit `Docs: the LVGL UI`.
