# Rhino Cyberspace Touch UI (Seeed Wio Tracker L2) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make every existing screen usable by touch on the Wio Tracker L2. That means tap, swipe and long-press, a `‹`/`⋯` status bar, an on-screen keyboard and four orientations. The M9 must keep working exactly as it does today.

**Architecture:** This follows Approach A from the spec:

- A pure `Gestures` recogniser and a `TouchResolver` in `lib/core/src/input/` turn touch samples into ordinary `rc::KeyEvent`s.
- Each frame, widgets fill a `TapMap` (`lib/core/src/ui/tap_map.*`) as they draw.
- `ui_task` resolves each gesture against the previous frame's map and sends the result through the existing `handleKey` path.
- Menus and confirms move to one shared `Overlay` widget. The keyboard (`Osk`) is a lib/core widget that `ui_task` docks in a second grid below the screen.

**Tech Stack:** C++17, pioarduino (Arduino 3.3 / ESP-IDF 5.5), LovyanGFX 1.2.35 (Wio panel only), Unity (native tests).

**Spec:** `docs/superpowers/specs/2026-10-26-cyberspace-touch-design.md`. Bring-up background: `.superpowers/wio-l2-hal-report.md`.

## Global Constraints

- **Build only: never flash, upload or open a serial port** (the USB port is in use). Hardware checks go into `docs/TESTING.md` (Task 12).
- Never call the live API (`api.cyberspace.online` or its RTDB) from tests, scripts or an agent. Run touch flows on `wio_l2_fake` and `m9_fake`.
- Every write comes from a user action. A tap or a keyboard key counts as a key press. A gesture never produces a write by itself: a drag or swipe only moves a selection or scrolls, and never sends `Select`. Nothing is retried.
- Never log passwords or tokens. Never log touch coordinates either, because they would reveal what was typed on the keyboard. Task 8 removes `RC_TOUCH_LOG`.
- Keep lib/core board-agnostic. Gestures, `TapMap`, the resolver, `Osk`, `Overlay` and the touch transform live in `lib/core` with no Arduino and no board headers. Device code outside `src/hal/` reaches hardware only through `src/hal/board.h`.
- Gesture thresholds (spec §3.2):
  - tap: up within 10 px of the down point and before 500 ms;
  - long-press: still down at 500 ms and within 10 px, emitted once;
  - drag: vertical movement past 10 px;
  - swipe: more than 40 px of travel in under 300 ms;
  - edge-back: a swipe right that starts within 16 px of the left edge.
- Wio buttons (spec §3.5):
  - BOOT: tap = Back, hold 600 ms = Home;
  - Wake: tap = screen off, hold = wake;
  - a Wake tap on a dark screen shows the screensaver.
- Touch is polled only while the screen is lit. It never wakes a dark screen.
- Orientations: Landscape, Landscape 180, Portrait, Portrait 180, listed in that order in Settings. The panel is 320×240 or 240×320. On the M9, `canRotate()` is `false` and `setRotation` is a no-op.
- Keyboard keys are about 32×36 px in landscape and 24×36 px in portrait. It has a letters page, `?123` and `#+=`. Shift is one-shot, and a double tap locks caps. The bottom row is `?123`, Space, `⌫`, Enter, `▼`.
- Out of scope: BLE keyboards, haptics, multi-touch, pinch zoom, LVGL.
- **The M9 stays unchanged.** `board::hasTouch()` is false on the M9, so `activeTaps()` stays null, nothing registers, the status bar draws as today, and the keyboard never shows. Every task that touches `src/` runs `pio run -e m9 && pio run -e m9_fake` and states why M9 behaviour is unchanged.
- Each task follows TDD where it has host tests, then commits with the given message followed by a blank line and `Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>`. Use `git commit -m "<message>" -m "Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"`.
- Verify:
  - `pio test -e native` is green (273 tests before this plan);
  - from Task 4 on, `pio run -e m9`, `-e m9_fake`, `-e wio_l2` and `-e wio_l2_fake` all build (`scripts/build-upload-monitor.sh -B` builds all four).

## Review Focus

1. **A finger already down when the screen wakes, or left down across a screen change:** no phantom tap or long-press fires on the new screen. Test: Task 1, `test_reset_ignores_finger_already_down`.
2. **A slow key press on the keyboard** (held past 500 ms) still types the character, exactly once. Test: Task 2, `test_resolve_osk_hold_is_tap`.
3. **The list changed between the frame and the tap** (a message arrived, a row went away): an out-of-range row selects nothing and sends no `Select`. Tests: Task 3, `test_overlay_activate_out_of_range`; Task 5, `test_list_select_out_of_range_refused`.
4. **Dragging past either end of a wrapping list** (menus, Feed) stops at the end. It never wraps to the other end. Test: Task 2, `test_resolve_drag_wrapping_list_clamps`.
5. **Out-of-range input:**
   - GT911 raw values beyond its 240×320 clamp to the frame edge;
   - a rotation outside 0–3 from NVS or web config falls back to Landscape.

   Tests: Task 1, `test_transform_clamps`; Task 11, `test_rotation_from_index_out_of_range`.

## File Structure

| File | Responsibility |
|---|---|
| `lib/core/src/input/gestures.{h,cpp}` | Gesture state machine (Task 1) |
| `lib/core/src/input/touch_transform.{h,cpp}` | Raw GT911 → frame coordinates per rotation (Task 1) |
| `lib/core/src/input/key_event.{h,cpp}` | Gains `Activate`, `Focus`, `PowerTap`, `ListId`, `index`/`target`/`then` (Task 2) |
| `lib/core/src/ui/home_tiles.{h,cpp}` | Home's tile layout and tile-grid key moves (touch boards) |
| `lib/core/src/ui/tap_map.{h,cpp}` | Per-frame hit targets and the active-map pointer (Task 2) |
| `lib/core/src/input/touch_resolve.{h,cpp}` | Gesture + `TapMap` → keys (Task 2) |
| `lib/core/src/ui/widgets/overlay.{h,cpp}` | The one menu/confirm box (Task 3) |
| `lib/core/src/ui/widgets/osk.{h,cpp}` | On-screen keyboard layout, state and drawing (Task 9) |
| `lib/core/src/ui/rotation.{h,cpp}` | Rotation labels, Settings order and parsing (Task 11) |
| `src/hal/board.h`, `src/hal/{m9,wio_l2}/…` | `wakeHint`, `canRotate`, `setRotation`, Wio buttons (Tasks 8 and 11) |
| `src/screens/*` | `Overlay` adoption, `Activate`/`Focus`, touch hints, `acceptsText` (Tasks 4 and 6) |
| `src/app/ui_task.cpp`, `src/ui/renderer.*` | Touch loop, keyboard docking, rotation (Tasks 8, 10 and 11) |
| `test/test_touch_input/`, `test/test_osk/` | New native suites; widget tests extend `test/test_widgets/` |

---

### Task 1: Gestures and the touch transform

**Files:**
- Create: `lib/core/src/input/gestures.{h,cpp}`, `lib/core/src/input/touch_transform.{h,cpp}`
- Test: `test/test_touch_input/test_main.cpp`

**Interfaces:**
- Produces (`gestures.h`, namespace `rc`):
  ```cpp
  constexpr int kTapSlopPx = 10;
  constexpr uint32_t kLongPressMs = 500;
  constexpr int kSwipeMinPx = 40;        // travel must exceed this…
  constexpr uint32_t kSwipeMaxMs = 300;  // …in less than this
  constexpr int kEdgeBackPx = 16;        // edge swipe starts at x < this
  enum class GestureKind : uint8_t { None, Tap, LongPress, Drag, Swipe, EdgeBack };
  enum class SwipeDir : uint8_t { Up, Down, Left, Right };
  struct Gesture {
    GestureKind kind = GestureKind::None;
    int16_t x = 0, y = 0;  // where the finger went down (every kind)
    int16_t dy = 0;        // Drag: movement since the previous Drag, or since the down point
    SwipeDir dir = SwipeDir::Up;
  };
  class Gestures {
   public:
    // One touch sample (down = finger on glass). True when it completes a gesture.
    bool feed(bool down, int16_t x, int16_t y, uint32_t ms, Gesture& out);
    // Forgets the touch in progress; a finger already down is ignored until it lifts.
    void reset();
  };
  ```
- Produces (`touch_transform.h`):
  ```cpp
  // Raw touch in the controller's native portrait (nativeW x nativeH) to frame
  // coordinates for rotation r, in quarter turns clockwise from the board's
  // landscape: 0 Landscape, 1 Portrait, 2 Landscape 180, 3 Portrait 180 (r mod 4).
  // Raw values outside the native size clamp to the edge first.
  void touchToFrame(int16_t rx, int16_t ry, int nativeW, int nativeH, uint8_t r,
                    int16_t& x, int16_t& y);
  ```
  With `W = nativeW`, `H = nativeH`, `lx = H-1-ry` and `ly = rx`, the four rotations are:
  - r0: `(lx, ly)`. This is today's `gt911::toLandscape`.
  - r1: `(W-1-ly, lx)`
  - r2: `(H-1-lx, W-1-ly)`
  - r3: `(ly, H-1-lx)`

  The panel's rotation direction is checked on hardware in Task 11. If portrait comes out mirrored, swap the r1 and r3 formulas and their tests.

- [x] **Step 1: Write the failing tests.** Add a helper `feedAll` that feeds a list of `{down, x, y, ms}` samples and collects the gestures.
  ```cpp
  void test_tap() {                       // down 100,100 @0; 104,103 @50; up @200
    // exactly one Tap{100,100}
  }
  void test_tap_late_is_not_tap()         // up @520 after a LongPress → no Tap
  void test_movement_cancels_tap()        // move to 111,100 (horizontal 11 px), up @100 → nothing
  void test_long_press_once() {
    // down @0; sample @499 → nothing; @500 → LongPress{x,y}; @700 → nothing; up → nothing
  }
  void test_movement_cancels_long_press() // move 11 px @300, hold to @900 → no LongPress
  void test_drag_vertical() {
    // down y=100 @0; y=105 → nothing; y=111 → Drag dy=11; y=115 → Drag dy=4; y=110 → Drag dy=-5
  }
  void test_swipe_four_directions()       // 41 px in 299 ms, each way → Swipe Up/Down/Left/Right
  void test_swipe_boundaries() {
    // 40 px in 200 ms → no Swipe; 41 px with up at 300 ms → no Swipe
  }
  void test_swipe_dominant_axis()         // dx=+45, dy=-30 → Right
  void test_edge_swipe_back() {
    // down x=15 → 60 in 200 ms → EdgeBack; down x=16 → same move → Swipe Right
  }
  void test_reset_ignores_finger_already_down() {
    // down @0, reset(), still down @100..@900 (crosses 500 ms), up → no gesture at all;
    // a fresh down/up afterwards → Tap
  }
  void test_transform_landscape_matches_bringup() {
    int16_t x, y;
    touchToFrame(0, 0, 240, 320, 0, x, y);     // → (319, 0)
    touchToFrame(239, 319, 240, 320, 0, x, y); // → (0, 239)
  }
  void test_transform_rotations_corners() {
    // For r = 0..3, the four raw corners map onto the four corners of the
    // rotated frame (320x240 for r 0 and 2, 240x320 for r 1 and 3), each one
    // exactly once; r2 of a point equals (319 - x0, 239 - y0) of its r0.
  }
  void test_transform_clamps() {
    int16_t x, y;
    touchToFrame(-5, 400, 240, 320, 0, x, y);  // → (0, 0)
    touchToFrame(300, -1, 240, 320, 1, x, y);  // x in [0,239], y in [0,319]
  }
  ```
- [x] **Step 2: Run** `pio test -e native -f test_touch_input`. Expected: the build fails because the headers are missing.
- [x] **Step 3: Implement** `Gestures` and `touchToFrame`. `Gestures` keeps the down point, the down time, the last y, a `moved` flag (past the slop in any axis), a `dragging` flag (past the slop vertically), `longFired`, and `armed` (`false` after `reset()` until a sample with `down == false`).
- [x] **Step 4: Run** `pio test -e native -f test_touch_input`, then `pio test -e native`. Expected: PASS.
- [x] **Step 5: Commit** `feat(core): touch gestures and touch transform`.

### Task 2: Key events, the tap map and the touch resolver

**Files:**
- Modify: `lib/core/src/input/key_event.{h,cpp}`, `src/screens/router.h` (replace `#include "input/keymap_m9.h"` with `#include "input/key_event.h"`; the M9 HAL keeps including `keymap_m9.h`)
- Create: `lib/core/src/ui/tap_map.{h,cpp}`, `lib/core/src/input/touch_resolve.{h,cpp}`
- Test: `test/test_touch_input/test_main.cpp`

**Interfaces:**
- Consumes: `Gesture`, `GestureKind`, `SwipeDir` (Task 1); `CellSize` (`ui/grid.h`).
- Produces (`key_event.h`). Append the new actions after `ScreenOff` so existing values don't move:
  ```cpp
  enum class Action { /* …existing…, */ ScreenOff, Activate, Focus, PowerTap };
  enum class ListId : uint8_t { Main, Menu, Drawer, Log };
  struct KeyEvent {
    Action action = Action::None;
    char32_t ch = 0;              // Char
    int16_t index = -1;           // Activate: the row; Focus: the text position
    uint8_t target = 0;           // Activate: a ListId; Focus: the screen's field id
    Action then = Action::None;   // Activate: sent once the selection has moved
  };
  ```
  `peeksScreen` returns true for `Select` and `PowerTap`. `wakesScreen` is unchanged: only `ScreenOff` wakes.
- Produces (`tap_map.h`):
  ```cpp
  enum class TargetKind : uint8_t { None, Row, Fixed, Field, Osk, Banner };
  struct Target {
    TargetKind kind = TargetKind::None;
    ListId list = ListId::Main;
    int16_t index = -1;          // Row: the item; Osk: the key; Field (from hit): text position
    KeyEvent tap, hold;          // Fixed; hold.action None means a hold acts as a tap
    uint8_t field = 0;           // Field
    int16_t first = 0, len = 0;  // Field row: positions first..first+len lie under it
  };
  struct ListInfo { ListId list; int selected, count; bool wraps; };
  class TapMap {
   public:
    void clear();                                  // targets and list infos
    void setOrigin(CellSize cell, int firstRow);   // for the addCells calls that follow
    void addCells(int c0, int r0, int c1, int r1, const Target& t);  // inclusive, stored in px
    void setList(ListId id, int selected, int count, bool wraps);
    const ListInfo* list(ListId id) const;         // null if not registered this frame
    int visibleRows(ListId id) const;              // Row targets registered for id
    CellSize cell() const;
    size_t size() const;
    // The last-registered target containing (x, y). For a Field, index =
    // first + the cell column under x (from the rect's left), capped at first + len.
    Target hit(int x, int y) const;
  };
  TapMap* activeTaps();          // null: no touch, so widgets register nothing
  void setActiveTaps(TapMap* m);
  ```
- Produces (`touch_resolve.h`):
  ```cpp
  struct TouchOutput {
    std::vector<KeyEvent> keys;
    int oskKey = -1;          // a keyboard key index for Osk::press
    bool hideBanner = false;
  };
  class TouchResolver {
   public:
    void resolve(const Gesture& g, const TapMap& m, TouchOutput& out);
    void reset();             // drops the drag carry
  };
  ```
  Resolution rules. `hit` is taken at the gesture's down point `(g.x, g.y)`.

  | Gesture | Target | Output |
  |---|---|---|
  | Tap | Row | `{Activate, index, target=list, then=Select}` |
  | LongPress | Row | `{Activate, index, target=list, then=Palette}` |
  | Tap | Fixed | `tap` |
  | LongPress | Fixed | `hold`, or `tap` when `hold` is None |
  | Tap / LongPress | Field | `{Focus, index=pos, target=field}` |
  | Tap / LongPress | Osk | `oskKey = index` |
  | Tap | Banner | `hideBanner` |
  | Drag | Row of list L | `carry += dy`. Each whole `cell().h` of carry is one step, and the remainder stays in `carry`. |
  | Swipe Up / Down | Row of list L | `max(1, visibleRows(L) - 1)` steps |
  | EdgeBack | any | `{Back}` |
  | anything else | | nothing |

  For steps on list L, the finger's direction decides which way the list moves. A finger moving down (`dy > 0`, or Swipe Down) goes towards earlier items. A finger moving up goes towards later ones.
  - **Wrapping list:** emit one `{Activate, clamp(selected ± steps, 0, count-1), target=L, then=None}`. Emit nothing when the index doesn't change.
  - **Non-wrapping list** (the log): emit `steps` × `Up` for earlier, or `Down` for later.

  A Drag that starts on a target that isn't a Row clears `carry`.

- [x] **Step 1: Write the failing tests.** They live in the same suite as Task 1.
  ```cpp
  void test_tap_map_cells_to_px() {
    TapMap m; m.setOrigin(CellSize{6, 12}, 10);
    m.addCells(2, 0, 4, 0, rowTarget(ListId::Main, 3));
    // hit(12,120) → Row 3; hit(11,120) and hit(12,119) and hit(30,120) → None
  }
  void test_tap_map_topmost_wins()        // overlapping Row then Fixed{Back}: hit → Fixed
  void test_tap_map_clear()               // after clear(): size()==0, list(Main)==nullptr
  void test_field_hit_position() {
    // Field{field=2, first=5, len=3} over cols 2..10 (cell w 6): x at col 4 → index 7;
    // col 9 → 8 (capped at first+len)
  }
  void test_resolve_row_tap_and_hold()    // Tap → Activate(3, Main, then Select); LongPress → then Palette
  void test_resolve_fixed() {
    // Fixed{tap=Back, hold=OpenHome}: Tap → Back; LongPress → OpenHome;
    // Fixed{tap=Palette}: LongPress → Palette
  }
  void test_resolve_field_focus()         // Focus, target 2, index 7
  void test_resolve_osk_hold_is_tap()     // Osk key 12: Tap → oskKey 12; LongPress → oskKey 12; no keys
  void test_resolve_banner()              // Tap → hideBanner, no keys
  void test_resolve_drag_log_steps() {
    // Log, wraps=false, cell h 12: dy +13 → [Up]; dy +11 → [Up]; dy -24 → [Down, Down]
  }
  void test_resolve_drag_wrapping_list_clamps() {
    // Main, wraps, selected 1, count 5, cell h 12: dy -72 (6 steps later) → Activate(4, then None);
    // selected 0: dy +36 → nothing
  }
  void test_resolve_swipe_pages()         // 6 Rows registered for Main, selected 0, count 20: Swipe Up → Activate(5)
  void test_resolve_edge_back()           // EdgeBack → [Back]
  void test_resolve_nothing_hit()         // Tap on empty map → no output
  void test_power_tap_peeks_not_wakes()   // peeksScreen(PowerTap) && !wakesScreen(PowerTap)
  ```
- [x] **Step 2: Run** `pio test -e native -f test_touch_input`. Expected: the build fails because `tap_map.h` and `touch_resolve.h` are missing.
- [x] **Step 3: Implement** `tap_map.cpp`. Store px rects in a `std::vector` (reserve 128). `activeTaps` is a file-static pointer. Then implement `touch_resolve.cpp` and the `key_event` changes, and switch `router.h` to `key_event.h`.
- [x] **Step 4: Run** `pio test -e native`, then `pio run -e m9 && pio run -e m9_fake && pio run -e wio_l2`. Expected: all pass. **M9:** `mapM9Key` never produces the new actions, and appending to `Action` changes no existing value.
- [x] **Step 5: Commit** `feat(core): tap map, touch resolver and portable key targets`.

### Task 3: The `Overlay` widget

**Files:**
- Create: `lib/core/src/ui/widgets/overlay.{h,cpp}`
- Test: `test/test_widgets/test_main.cpp`

**Interfaces:**
- Consumes: `ListView`, `ListItem`, `ItemTone`; `KeyEvent`, `ListId` (Task 2).
- Produces:
  ```cpp
  enum class OverlayPlace : uint8_t { BottomRight, Centre, Bottom };
  class Overlay {
   public:
    enum class Result : uint8_t { None, Picked, Closed };
    void open(std::u32string title, std::vector<ListItem> items, int selected = 0);
    // Items "Cancel" (selected) and action in ItemTone::Error.
    void confirm(std::u32string title, std::u32string action);
    void close();
    bool isOpen() const;
    int selected() const;
    const std::u32string& title() const;
    // Up/Down move and wrap; Select/Right pick (and close); Back/Left close;
    // Activate with target Menu selects index (if in range) then applies `then`.
    // Any other key: None, still open.
    Result key(const KeyEvent& k);
    // Within rows [top, bottom) and columns [0, g.cols() - rightReserve).
    void render(Grid& g, int top, int bottom, int rightReserve, OverlayPlace p, const Theme& t) const;
  };
  ```
- Placements copy today's code exactly, with `n` = the number of items:
  - **BottomRight** (`room.cpp`, `entry.cpp`, `feed_list.cpp`):
    - `h = min(n, bottom-2)`, `boxTop = bottom-h-1`;
    - `width = min(cols-2, 30)`;
    - `left = max(1, cols-width-1-rightReserve)`;
    - the title bar is on `boxTop` (barFg/barBg across `width`, text at `left+1`);
    - the list is `[boxTop+1, bottom)`.
  - **Centre** (`composer.cpp`), where `bottom` is the grid's row count:
    - `width = min(cols-2, max(20, title.size()+2))`, `left = max(1, (cols-width)/2)`;
    - `boxTop = max(1, (bottom-n-1)/2)`;
    - the list is `[boxTop+1, min(bottom, boxTop+1+n))`.
  - **Bottom** (`settings.cpp` confirm):
    - the title goes in `t.error` on `t.bg` at col 1, row `bottom-3`;
    - the list is `[bottom-2, bottom)` in cols `1 … cols-2`.

- [x] **Step 1: Write the failing tests.**
  ```cpp
  void test_overlay_confirm_defaults_cancel_and_wraps() {
    Overlay o; o.confirm(U"Log out?", U"Log out");
    // selected 0; key(Up) → None, selected 1; key(Select) → Picked, !isOpen(), selected 1
  }
  void test_overlay_back_and_left_close()  // Back → Closed; reopen; Left → Closed
  void test_overlay_activate_picks()       // 3 items: Activate{index 2, target Menu, then Select} → Picked, selected 2
  void test_overlay_activate_out_of_range() {
    // Activate{index 9, Menu, Select} → None, still open, selection unchanged;
    // Activate{index 1, target Main} → None (another list's row)
  }
  void test_overlay_render_bottom_right() {
    // 53x20 grid, 3 items "a","b","c", title "menu", render(g, 1, 18, 0, BottomRight):
    // row 14 cols 22..51 have bg barBg and text " menu" from col 23; rows 15..17 hold a,b,c;
    // row 15 outlined (selection)
  }
  void test_overlay_render_centre_and_bottom() // title cells at the formulas above
  ```
- [x] **Step 2: Run** `pio test -e native -f test_widgets`. Expected: the build fails because `overlay.h` is missing.
- [x] **Step 3: Implement** `Overlay`. It holds a `ListView`, the title, `open_`, and the label list.
- [x] **Step 4: Run** `pio test -e native`. Expected: PASS.
- [x] **Step 5: Commit** `feat(ui): shared Overlay menu and confirm box`.

### Task 4: Screens use `Overlay` (M9 behaviour unchanged)

**Files:**
- Modify: `src/screens/room.{h,cpp}`, `src/screens/entry.{h,cpp}`, `src/screens/composer.{h,cpp}`, `src/screens/feed_list.cpp`, `src/screens/screens.h` (`FeedListScreen`, `SettingsScreen`), `src/screens/settings.cpp`

**Interfaces:**
- Consumes: `Overlay`, `OverlayPlace` (Task 3).
- Produces: no new API. Each screen's `menuList_` + `menuTitle_` (and FeedList's `paletteList_`, Settings's `confirmMenu_`) becomes one `Overlay box_`.
  - `RoomScreen` and `EntryScreen` already have a nested `enum class Overlay`. Rename it to `enum class View` (`View::None, Menu, Reader, Link`) so it doesn't hide `rc::Overlay`.
  - The `Menu` enums, `menuKeys_` and `pickMenu()` stay. `menuKey()` becomes `switch (box_.key(k))`: `Picked` → `pickMenu()` using `box_.selected()`; `Closed` → leave the menu.
  - Placements:
    - Room: `BottomRight`, with the drawer width as `rightReserve`;
    - Entry and FeedList: `BottomRight`, reserve 0;
    - Composer: `Centre` with `top 0, bottom g.rows()`;
    - Settings: `Bottom` with `confirm(question, action)`.
  - Composer's, Settings's and `BackupPicker`'s confirms use `Overlay::confirm` (BackupPicker: `Bottom`, Cancel preselected as today). The Settings time-zone list and BackupPicker's file list stay `ListView`s. Room's and Entry's ConfirmDelete/ConfirmReport keep their own titles and items through `open`, so their key mapping (`cancel`, `delete`, …) is unchanged.

- [x] **Step 1: Convert** Room and Entry. Keep each menu's rows, title text, `menuKeys_` order and hints exactly as they are.
- [x] **Step 2: Convert** Composer (Actions, ConfirmPost, ConfirmDiscard), the FeedList palette, and the Settings confirms (Logout, Reset).
- [x] **Step 3: Run** `pio test -e native`, then `scripts/build-upload-monitor.sh -B`. Expected: green, and all four envs build.
- [x] **Step 4: Check M9 behaviour.** Diff each converted screen's old `menuKey` against `Overlay::key`: Up/Down wrap, Select/Right pick, Back/Left close, and Cancel is the default. `test_overlay_render_*` pins the drawing. Do not flash. Task 12 adds the on-device check (every menu and confirm in Room, Entry, Composer, Feed and Settings) to TESTING.md.
- [x] **Step 5: Commit** `refactor(screens): menus and confirms use Overlay`.

### Task 5: Widgets register tap targets; touch status bar; new glyphs

**Files:**
- Modify: `lib/core/src/ui/widgets/{list_view,entry_list,drawer,log_view,input_line,text_area,status_bar,banner,overlay}.{h,cpp}`
- Modify: `lib/core/src/ui/glyph_set.h`, `src/ui/fonts/cell*.h` (regenerated)
- Modify: `src/app/app_state.{h,cpp}` (`drawStatus`)
- Test: `test/test_widgets/test_main.cpp`, `test/test_glyphs/test_main.cpp`

**Interfaces:**
- Consumes: `TapMap`, `activeTaps()`, `Target`, `ListId` (Task 2).
- Produces:
  ```cpp
  bool ListView::select(int i);      // false (unchanged) when i is out of range
  void ListView::setListId(ListId);  // default Main; Overlay's list uses Menu
  bool EntryList::select(int i);
  bool Drawer::select(int i);
  bool LogView::selectMessage(int ordinal);  // ordinal among distinct message ids, oldest first
  int LogView::messageCount() const;
  void InputLine::setFieldId(uint8_t id);
  void InputLine::setCursor(size_t pos);     // clamped to the text length
  void TextArea::setFieldId(uint8_t id);
  void TextArea::setCursor(size_t pos);
  struct StatusTouch { bool on = false, back = true, menu = false; };
  void renderStatusBar(Grid&, const Theme&, std::u32string_view title, int count, char32_t connMarker,
                       int wifiBars, int batteryPct, std::string_view clock, StatusTouch touch = {});
  // app_state.h: menu = the screen has a Ctrl menu (draws ⋯)
  void drawStatus(Grid& g, const Theme& t, std::u32string_view title, int count, bool menu = false);
  ```
- Registration happens only when `activeTaps()` is non-null, inside each widget's `render`:

  | Widget | Registers |
  |---|---|
  | `ListView` | a Row per visible item, across the list width; `setList(id, sel, n, true)` |
  | `EntryList` | a Row per entry over both of its rows; `setList(Main, sel, n, true)` |
  | `Drawer` | a Row per person; `setList(Drawer, sel, n, true)` |
  | `LogView` | a Row per visible non-divider line (`index` = the message's ordinal); `setList(Log, -1, messageCount(), false)` |
  | `InputLine` | a Field over the text cells after the prompt, with `first` = the scroll start and `len` = the visible characters. It registers when disabled too, because a tap focuses it. |
  | `TextArea` | a Field per visible segment (`first = seg.start`, `len = seg.len`) |
  | `Banner` | a `Banner` target over its row while visible |
  | `Overlay` | first a `Fixed{tap=Back}` over the whole grid (outside the box closes it), then its list as `Menu` Rows |
  | Status bar, with `touch.on` | `‹` at col 0 when `back`: `Fixed{tap=Back, hold=OpenHome}` over cols 0–3 of row 0. `⋯` as the last cell before the trailing space when `menu`: `Fixed{tap=Palette}` over the last 4 cols. The title starts at col 2 and is cut to fit. |

- `drawStatus` passes `StatusTouch{board::hasTouch(), router::current() != router::Id::Home, menu}`.
- Glyphs: add `0x2039 ‹`, `0x22EF ⋯`, `0x25BC ▼`, `0x21E7 ⇧` and `0x232B ⌫` to `kExtraGlyphs`. Regenerate `src/ui/fonts/cell*.h` with `scripts/bdf2cells.py` from Terminus 4.49.1, downloaded outside the repo and run with `python3 -I`. For any of these that Terminus lacks, add a `kGlyphAliases` entry: `⋯→… (0x2026)`, `⇧→↑ (0x2191)`, `⌫→← (0x2190)`.

- [x] **Step 1: Write the failing tests.** In each one, `setActiveTaps(&m)` first and `setActiveTaps(nullptr)` in `tearDown`.
  ```cpp
  void test_list_registers_rows() {
    // spec §5: 5 items, sel 0, render rows 1..5 → a tap on the fourth row (row 4) → Row{Main, 3}
    // 10 items, render rows 2..6 (height 5), sel 7 → first shown is 3:
    // hit at row 2 → Row{Main, 3}; list(Main) = {7, 10, wraps}
  }
  void test_list_select_out_of_range_refused() // select(10) and select(-1) → false, selected() unchanged
  void test_entry_list_row_spans_two_rows()     // both rows of entry 1 → Row{Main, 1}
  void test_log_registers_messages() {
    // lines a, a, divider, b: hits on both "a" rows → index 0; divider → None; "b" → 1;
    // selectMessage(1) → selectedId()=="b"; selectMessage(2) → false
  }
  void test_input_field_hit_scrolled() {
    // 40 cols, prompt "> ", 60 chars, cursor at end → first = 60-38+1 = 23:
    // hit at col 2 → Field index 23
  }
  void test_text_area_field_rows()      // "ab\ncd": hit at row 2 col 1 → index 4; past end of row 1 → 2
  void test_status_bar_touch_layout() {
    // StatusTouch{true, true, true}: cell(0,0)=='‹', title from col 2, cell(cols-2,0)=='⋯';
    // hit(1,1) → Fixed tap Back hold OpenHome; hit at the ⋯ cell → Fixed tap Palette;
    // StatusTouch{true, false, false}: no ‹, no ⋯
  }
  void test_overlay_registers_taps()   // hit inside the box row 2 → Row{Menu,1}; outside → Fixed{Back}
  void test_banner_tap_target()        // visible → Banner target; expired → None
  void test_no_registration_without_active_map() // activeTaps()==nullptr: render, no crash, map untouched
  ```
  `test_status_bar_layout` (`touch` defaulted off) must keep passing unchanged. In `test_glyphs`, add `test_touch_glyphs_present`: each new code point has a bitmap or an alias in both fonts.
- [x] **Step 2: Run** `pio test -e native -f test_widgets`. Expected: the build fails because the new methods are missing.
- [x] **Step 3: Implement** the registrations, the `select*`/`setCursor`/`setFieldId` methods, `StatusTouch`, `drawStatus`, and the glyph set and fonts.
- [x] **Step 4: Run** `pio test -e native`, then `scripts/build-upload-monitor.sh -B`. Expected: green, and all four envs build. **M9:** with `hasTouch()` false, `StatusTouch.on` is false and nothing registers. The status bar row matches today's (`test_status_bar_layout`).
- [x] **Step 5: Commit** `feat(ui): widgets register tap targets; touch status bar`.

### Task 6: Screens handle `Activate` and `Focus`; touch hints

**Files:**
- Modify: `src/screens/router.h` (`Screen::acceptsText`), `src/screens/draw.h`, and every screen in `src/screens/` that has a list, a field or a hint
- Test: `test/test_keymap_web/test_main.cpp`

**Interfaces:**
- Consumes: `Action::Activate`/`Focus` and `KeyEvent::index`/`target`/`then` (Task 2); `select`, `selectMessage`, `setCursor` and `setFieldId` (Task 5); `Overlay::key` (Task 3) for Menu rows.
- Produces:
  ```cpp
  // router.h, class Screen: true while a focused text field takes typing (docks the keyboard).
  virtual bool acceptsText() const { return false; }
  // draw.h: keys text on keyed boards, touch text when board::hasTouch().
  inline void drawHint(Grid& g, const Theme& t, std::u32string_view keys, std::u32string_view touch);
  ```
- `Activate` handling: if `k.target` names one of the screen's lists and `select(k.index)` succeeds, the screen calls `onKey(KeyEvent{k.then})`. When `then` is None it just moves the selection. A failed `select` does nothing.
  - Home, Settings, Setup, RoomList and CmailList: `Main`. This includes the Settings time-zone list and the `BackupPicker` file list (Settings and Setup) while they are open.
  - FeedList: `Main`. Selecting the last row with `more_ && !asked_` calls `requestMore()`, because a drag never reaches the Down-at-end path.
  - Room: `Log` closes the drawer first (as Left does), then `selectMessage`. `Drawer` uses `drawer_.select`.
  - Entry: `Log`.
  - Any screen with `box_` open passes the key to `box_.key`.
- `Focus` handling, with field ids from each screen's own enum:
  - Room, Entry and CmailList (one input, id 0): `setCursor(k.index)`.
  - LoginForm: email 0, password 1. The masked line gets `setFieldId(1)`. Set `focus_`, then `setCursor`.
  - Composer: ids are `Field` values (`kTitle`…`kCw`). Set `focus_`, then `setCursor`. The checkboxes register Field targets (len 0) over `[ ] Public` and `[ ] NSFW`, and a `Focus` on them sets focus and toggles, as Select does.
- `acceptsText()` is true for:
  - Room and Entry when no menu, reader or link view is open (reason mode counts);
  - Composer when `focus_` is Title, Body, Topics or CW, no menu is open and nothing is posting;
  - LoginForm when `!busy_`;
  - CmailList when `editing_ && !busy_`.
- `drawStatus(…, menu=true)` on Room, Conversation, Entry, Composer and the three FeedList kinds.
- Touch hints. The key text stays exactly as it is today:

  | Screen / state | Touch hint |
  |---|---|
  | Home | `tap to open` |
  | Setup | `tap to select` |
  | RoomList, CmailList | `tap to open  swipe to scroll` |
  | CmailList editing | `type a username  enter: start` (busy: `starting…`) |
  | Settings / confirm | `tap to select` / `tap to confirm  tap outside: cancel` |
  | Any open menu | `tap to choose  tap outside: close` |
  | FeedList | `tap to open  hold: menu` (loading more: unchanged) |
  | FeedList empty | `couldn't load: ⋯, Refresh` replaces `couldn't load: Ctrl, Refresh` |
  | Room / Conversation | `tap the line to type  tap a message for actions` |
  | Room drawer | `tap a name for actions` |
  | Room reader / link | `swipe to scroll  ‹ close` / `‹ close` |
  | Room reason mode | `type a reason (optional)  enter: report` |
  | Entry, Composer | the same pattern, `⋯` for "ctrl" and `‹` for "back" |
  | LoginForm | `tap a field  enter: next/sign in` |
  | ComingSoon | `‹ home` |

- [x] **Step 1: Write the failing test** in `test_keymap_web`:
  ```cpp
  void test_m9_keymap_never_touch_actions() {
    for (int raw = 0; raw < 256; ++raw)
      for (bool early : {false, true}) {
        Action a = mapM9Key(uint8_t(raw), early).action;
        TEST_ASSERT_TRUE(a != Action::Activate && a != Action::Focus && a != Action::PowerTap);
      }
  }
  ```
  It passes once Task 2's enum exists. It guards the M9 against ever reaching the new paths.
- [x] **Step 2: Run** `pio test -e native -f test_keymap_web`. Expected: PASS. This is a guard, not a red-green test.
- [x] **Step 3: Implement** `acceptsText`, the `drawHint` overload, and the `Activate`/`Focus` handling, screen by screen.
- [x] **Step 4: Run** `pio test -e native`, then `scripts/build-upload-monitor.sh -B`. Expected: green, and all four envs build. **M9:** no M9 key maps to `Activate`/`Focus` (Step 1), `drawHint` picks the key text when `hasTouch()` is false, and `acceptsText` is read only by Task 10's docking.
- [x] **Step 5: Commit** `feat(screens): tap rows and fields, touch hints`.

### Task 7: Home tiles on touch boards

**Files:**
- Create: `lib/core/src/ui/home_tiles.{h,cpp}`
- Modify: `src/screens/home.cpp`, `src/screens/screens.h` (HomeScreen members)
- Test: `test/test_widgets/test_main.cpp`

**Interfaces:**
- Consumes: `TapMap`, `activeTaps()`, `Target`, `ListId::Main` (Task 2); `Activate` handling on `Main` (Task 6); `drawHint(g, t, keys, touch)` (Task 6).
- Produces (namespace `rc`):
  ```cpp
  struct TileRect { int c0, r0, c1, r1; };  // inclusive cells
  // count tiles in 2 columns, row-major, filling rows 1..rows-2 (between the
  // status bar and the hint). Empty when a tile would be under 3 rows or 10 cols.
  std::vector<TileRect> homeTiles(int cols, int rows, int count);
  // Tile-grid moves for keys: Up/Down by a row (±2), Left/Right within a row; clamps, no wrap.
  int tileMove(int selected, int count, Action a);
  ```
- Behaviour (the user approved this layout):
  - Only when `board::hasTouch()` and `homeTiles(...)` is non-empty; otherwise Home draws today's list, unchanged (the M9 path).
  - Six tiles, in order: `messages` (sub-line `#<lastRoom>`), `rooms`, `c-mail` (`N unread`), `feed` (`N unread`; opens like today's Feed item, straight to the newest unread notice), `settings`, `mentions` (label `N mentions`, opens the last room like `messages`). The user/active-rooms fields are not shown in tile mode; `@user` stays in the status bar.
  - Each tile: `Grid::outline` in `t.dim`; label centred on the tile's middle row, sub-line under it in `t.dim`. A tile with unread items draws its label in `t.selectBg`. The selected tile is filled `t.selectBg` with `t.selectFg` text and an outline in `t.selectBg`.
  - Each tile registers `addCells(c0, r0, c1, r1, Target{Row, Main, index=i})`, and Home calls `setList(Main, selected, 6, /*wraps*/false)`. A tap opens the tile (Task 6's `Activate` → `Select`).
  - Keys in tile mode: Up/Down/Left/Right use `tileMove`; Select opens. (Right no longer opens in tile mode.)
  - Touch hint: `tap to open` (from Task 6).

- [x] **Step 1: Write the failing tests** in `test_widgets`:
  ```cpp
  void test_home_tiles_fit() {
    // (40,15,6) and (53,20,6) and portrait (30,20,6): 6 tiles; each inside cols 0..cols-1, rows 1..rows-2;
    // no two overlap; every tile >= 3 rows and >= 10 cols; tiles 0,2,4 share c0 and 1,3,5 share c0; row-major
  }
  void test_home_tiles_too_small()  // homeTiles(16, 8, 6) → empty
  void test_tile_move() {
    // count 6: tileMove(0,6,Down)=2; tileMove(4,6,Down)=4; tileMove(1,6,Left)=0; tileMove(0,6,Left)=0;
    // tileMove(0,6,Right)=1; tileMove(1,6,Right)=1; tileMove(3,6,Up)=1; tileMove(1,6,Up)=1
  }
  ```
- [x] **Step 2: Run** `pio test -e native -f test_widgets`. Expected: it fails to build.
- [x] **Step 3: Implement** `homeTiles`, `tileMove`, and the tile mode in `home.cpp`.
- [x] **Step 4: Run** `pio test -e native` and `scripts/build-upload-monitor.sh -B`. Expected: green, all four envs build; the M9 Home is unchanged (`hasTouch()` false).
- [x] **Step 5: Commit** `feat(home): large tiles on touch boards`.

### Task 8: Touch in the UI loop; Wio buttons; the wake hint

**Files:**
- Modify: `src/app/ui_task.cpp`
- Modify: `src/hal/board.h` (`wakeHint`), `src/hal/m9/board_m9.cpp`
- Modify: `src/hal/wio_l2/board_wio_l2.cpp`, `platformio.ini` (drop `-DRC_TOUCH_LOG=1` from `[env:wio_l2]`)

**Interfaces:**
- Consumes: `Gestures` (Task 1); `TapMap`, `setActiveTaps`, `TouchResolver`, `TouchOutput` (Task 2); `Banner::hide`.
- Produces:
  ```cpp
  // board.h: the saver's last line. M9 "hold \u25CF to wake"; Wio L2 "hold Wake to wake".
  const char* wakeHint();
  ```
- `ui_task` behaviour:
  - `start()`: `setActiveTaps(board::hasTouch() ? &taps : nullptr)`.
  - Each frame, **first** (before `pollKey`): if `power == On` and `hasTouch()`, read one sample, feed `gestures`, and if a gesture completes, call `resolver.resolve(g, taps, out)`.
    - Apply `hideBanner` → `topBanner.hide()`.
    - Each of `out.keys` → `handleKey`.
    - `oskKey` is ignored until Task 10.

    Touch comes before buttons so a gesture always resolves against the map of the frame on screen.
  - Before `router::screen().render`: `taps.clear(); taps.setOrigin(cellSize(textSize), 0)`.
  - After the screen, banner and status bar are drawn into the grid (before `renderer.draw`): `taps.mapRows(0, rowSpans(grid, cellSize(textSize).h, bigCellSize(textSize).h))`, so taps on the Feed's Big rows land where they are drawn.
  - `gestures.reset()` and `resolver.reset()` when going dark, entering the saver, waking, and on every `router` screen change (compare `router::current()` before and after the handled keys and events).
  - `PowerTap` while lit acts as `ScreenOff` (it turns the screen off). While dark, `peeksScreen` already makes it peek.
  - `drawSaver` draws `board::wakeHint()` instead of the literal.
- Wio buttons (`scanButtons`):
  - BOOT: `kBootHoldMs = 600`, tap → `Back`, hold → `OpenHome`.
  - Wake: tap → `PowerTap`, hold → `ScreenOff` (unchanged).
  - Delete `logTouch` and the `RC_TOUCH_LOG` blocks, and update the comment on the mapping.

- [x] **Step 1: Implement** the changes above. There is no new host test, because the logic is covered by Tasks 1–2.
- [x] **Step 2: Run** `pio test -e native`, then `scripts/build-upload-monitor.sh -B`. Expected: green, and all four envs build. Also check that `grep -rn "RC_TOUCH_LOG\|\[touch\] down" src platformio.ini` finds nothing.
- [x] **Step 3: Check M9 behaviour.** `hasTouch()` is false, so the touch block never runs and `activeTaps()` stays null. M9 `wakeHint()` returns today's string. Neither M9 button mapping produces `PowerTap`.
- [x] **Step 4: Commit** `feat(app): touch input in the UI loop; Wio BOOT and Wake buttons`.

### Task 9: The on-screen keyboard (`Osk`)

**Files:**
- Create: `lib/core/src/ui/widgets/osk.{h,cpp}`
- Test: `test/test_osk/test_main.cpp`

**Interfaces:**
- Consumes: `Grid`, `TextSize`, `cellSize` (`ui/grid.h`); `TapMap`/`activeTaps` and `TargetKind::Osk` (Task 2); `KeyEvent`.
- Produces:
  ```cpp
  enum class OskPage : uint8_t { Letters, Symbols, More };
  enum class OskKind : uint8_t { Char, Shift, Page, Space, Backspace, Enter, Hide };
  struct OskKey {
    OskKind kind; char32_t ch;     // Char: the lower-case/base character
    std::u32string label;
    OskPage to;                    // Page: the page it opens
    int col, row, w, h;            // cells within the keyboard grid
  };
  struct OskOutput { KeyEvent key; bool hide = false; };  // key.action None: nothing for the screen
  constexpr uint32_t kShiftLockMs = 400;                   // second Shift tap within this locks caps
  class Osk {
   public:
    static int keyRows(TextSize s);  // 2 Comfortable (32 px), 3 Compact (36 px)
    static int rows(TextSize s);     // 4 * keyRows(s)
    void layout(int cols, TextSize s);  // rebuilds keys() for the current page
    void reset();                       // Letters page, Shift off (each time it is shown)
    const std::vector<OskKey>& keys() const;
    OskPage page() const;
    bool shifted() const;
    bool capsLock() const;
    OskOutput press(int i, uint32_t nowMs);
    // Fills g (a rows()-row grid) and registers one Osk target per key.
    void render(Grid& g, const Theme& t) const;
  };
  ```
- Layout. A unit is `cols / 10` cells, with the leftover columns split evenly as a left margin, and a key is one unit wide unless stated. That gives 5 cells (30 px) at 53 cols, 4 (32 px) at 40 cols Comfortable landscape, 4 (24 px) at 40 cols Compact portrait, and 3 (24 px) at 30 cols. Key height is `keyRows(s)`. Rows, top to bottom:

  | Page | Row 0 | Row 1 | Row 2 | Row 3 |
  |---|---|---|---|---|
  | Letters | `qwertyuiop` | `asdfghjkl`, shifted right half a unit | `⇧` `zxcvbnm` `,` `.` | `?123`(2) `space`(4) `⌫` `enter`(2) `▼` |
  | Symbols | `1234567890` | `-/:;()$&@"` | `#+=`(2) `.,?!'_*+` | `ABC`(2) `space`(4) `⌫` `enter`(2) `▼` |
  | More | `[]{}#%^=\|` | `` ~<>`£¥§°·• `` | `123`(2) `…✓±×÷¿¡é` | `ABC`(2) `space`(4) `⌫` `enter`(2) `▼` |

- Events:
  - Char → `{Char, ch}`, upper-cased while shifted or locked.
  - Space → `{Char, U' '}`.
  - `⌫` → `{Backspace}`.
  - enter → `{Select}`, which inserts a newline in the Composer Body as Select does today.
  - `▼` → `hide = true`.
  - Shift and Page keys emit nothing.
- Shift:
  - a tap shifts for one character;
  - a second tap within 400 ms locks caps;
  - a tap while shifted (after 400 ms) or locked turns it off;
  - one-shot Shift clears after the next Char.

  Page keys keep the Shift state.
- Drawing: the keys are filled with `t.inputBg`, with the label centred in `t.fg` and an outline in `t.dim`. Shift is drawn in `selectBg`/`selectFg` while shifted or locked. Everything else is `t.bg`.

- [x] **Step 1: Write the failing tests.**
  ```cpp
  void test_osk_rows()                    // rows(Comfortable)==8, rows(Compact)==12
  void test_osk_layout_landscape_compact() {
    Osk k; k.layout(53, TextSize::Compact);
    // 'q' at col 1 (margin (53-50)/2), w 5, h 3, row 0; 'a' at col 1+2 (half unit), row 3;
    // space w 20; ▼ is the last key on row 9
  }
  void test_osk_layout_portrait()         // layout(40, Compact): key w 4; layout(30, Comfortable): w 3, h 2
  void test_osk_page_switching()          // ?123 → Symbols (first key '1'); #+= → More; ABC → Letters
  void test_osk_shift_one_shot_and_lock() {
    // ⇧ @0 then 'a' → 'A', next 'a' → 'a'; ⇧ @1000, ⇧ @1200 → capsLock, 'a','b' → 'A','B';
    // ⇧ again → off
  }
  void test_osk_key_events()              // space → Char ' '; ⌫ → Backspace; enter → Select; ▼ → hide
  void test_osk_covers_printable_ascii() {
    // union of Char keys over the three pages (letters with and without shift) ⊇ 0x21..0x7E
  }
  void test_osk_registers_targets()       // render with activeTaps set: hit on 'q' → Osk{index of 'q'}
  ```
- [x] **Step 2: Run** `pio test -e native -f test_osk`. Expected: the build fails because `osk.h` is missing.
- [x] **Step 3: Implement** `Osk`.
- [x] **Step 4: Run** `pio test -e native`. Expected: PASS.
- [x] **Step 5: Commit** `feat(ui): on-screen keyboard`.

### Task 10: Dock the keyboard

**Files:**
- Modify: `src/ui/renderer.{h,cpp}`, `src/app/ui_task.cpp`

**Interfaces:**
- Consumes: `Osk`, `OskOutput` (Task 9); `Screen::acceptsText` (Task 6); `TouchOutput::oskKey` (Task 2).
- Produces:
  ```cpp
  // Renderer: draws g's dirty rows starting at grid row firstRow of the panel.
  void draw(Grid& g, TextSize s, int firstRow = 0);
  ```
- `ui_task` behaviour:
  - It owns `Osk osk`, `Grid oskGrid(1, 1)` and `bool oskShown`.
  - **Show:** after a handled `Focus` key, if `router::screen().acceptsText()`, set `oskShown`, call `osk.reset()`, and resize.
  - **Hide:** on `hide` from `osk.press`, when `acceptsText()` turns false after keys or events, on a screen change, and when going dark.
  - **Resize:** `resizeGrid()` gives the screen grid `rows - Osk::rows(ts)` rows while shown, and `oskGrid` the remaining `Osk::rows(ts)` rows. It calls `osk.layout(cols, ts)`. Showing or hiding sets `needResize`.
  - **Render:**
    1. `taps.setOrigin(cell, 0)`, then the screen and banner;
    2. while shown, `taps.setOrigin(cell, grid.rows())`, `osk.render(oskGrid, t)`, then `renderer.draw(oskGrid, ts, grid.rows())`.
  - **Touch:** `out.oskKey >= 0` → `osk.press(i, millis())`. A `key` with an action goes to `handleKey`. `hide` hides the keyboard.

- [x] **Step 1: Implement** the `Renderer` offset, then the docking.
- [x] **Step 2: Run** `pio test -e native`, then `scripts/build-upload-monitor.sh -B`. Expected: green, and all four envs build. **M9:** no `Focus` key is ever produced, so `oskShown` stays false and `draw`'s default offset is 0.
- [x] **Step 3: Check on `wio_l2_fake`** if a board is at hand; otherwise leave it to the Task 12 checklist (no board: left to Task 12):
  - tap the Room input and the keyboard docks;
  - type, use `⌫` and enter;
  - `▼` hides the keyboard;
  - Back with the keyboard up leaves the room and the keyboard goes.
- [x] **Step 4: Commit** `feat(app): dock the on-screen keyboard`.

### Task 11: Orientation

**Files:**
- Create: `lib/core/src/ui/rotation.{h,cpp}`
- Modify: `src/hal/board.h`, `src/hal/m9/board_m9.cpp`, `src/hal/wio_l2/{board_wio_l2,panel_nv3031b,touch_gt911}.{h,cpp}`
- Modify: `lib/core/src/config/settings_model.h`, `lib/core/src/config/backup_yaml.cpp`, `test/test_backup_yaml/test_main.cpp`, `docs/superpowers/specs/2026-10-26-cyberspace-backup-design.md`, `src/store/settings.{h,cpp}`, `src/app/messages.h`, `src/app/ui_task.{h,cpp}`, `src/ui/renderer.{h,cpp}`
- Modify: `src/screens/settings.cpp`, `src/screens/screens.h`, `src/web/web_server.cpp`, `src/web/page.h`
- Test: `test/test_touch_input/test_main.cpp`

**Interfaces:**
- Consumes: `touchToFrame` (Task 1); `Overlay` (Task 3).
- Produces:
  ```cpp
  // rotation.h (rc). r: 0 Landscape, 1 Portrait, 2 Landscape 180, 3 Portrait 180.
  constexpr uint8_t kRotationOrder[4] = {0, 2, 1, 3};  // the Settings / web order
  const char* rotationLabel(uint8_t r);  // "Landscape", "Portrait", "Landscape 180", "Portrait 180"
  uint8_t rotationFromIndex(int i);      // out of 0..3 → 0
  bool rotationPortrait(uint8_t r);      // r odd
  // board.h
  bool canRotate();
  void setRotation(uint8_t r);           // panel scan and touch transform together
  // ui_task.h (rc::ui): applies at once, saves, reflows the grid.
  void setRotation(uint8_t r);
  // Settings: uint8_t rotation = 0;  NVS key "rot", saved by saveUiPrefs, loaded via rotationFromIndex.
  // EvtType::RotationChanged: code = r (from web config).
  // Renderer: re-reads the panel size; reallocates only if w*h grows.
  void resize();
  ```
- M9: `canRotate()` returns `false` and `setRotation` does nothing.
- Wio:
  - `nv3031b::setRotation(r)` → `lcd.setRotation(r)`. `offset_rotation 1` stays.
  - `gt911::read` returns raw native coordinates; `toLandscape` is deleted.
  - `board::readTouch` applies `rc::touchToFrame(raw, PANEL_NATIVE_W, PANEL_NATIVE_H, rot)`.
  - `panelWidth`/`panelHeight` swap when `rotationPortrait(rot)`, and `pushRows` uses `panelWidth()`.
- Boot: `ui::start()` calls `board::setRotation(app().cfg.rotation)` before `renderer.begin()` when `canRotate()`.
- `ui::setRotation(r)` and `RotationChanged` both do the following: `cfg.rotation = r`, `settings::saveUiPrefs`, `board::setRotation`, `renderer.resize()`, `gestures.reset()`, then `needResize`.
- Settings screen: the menu is already a dynamic `menuItems()` list of `Item` ids. Add `kRotation` right after `kZone`, only when `canRotate()`.
  - The row is labelled `Orientation: <label>`.
  - Select opens `box_.open(U"orientation", the four labels in kRotationOrder, the current one selected)`, and Picked calls `ui::setRotation(kRotationOrder[i])`.
- Web config:
  - GET `/api/prefs` adds `"rotation": r` only when `board::canRotate()`.
  - POST accepts an integer 0–3, which posts `RotationChanged`; anything else returns `400 "Orientation: 0 to 3"`.
  - `page.h` adds an Orientation `<select>` (values 0, 2, 1, 3 in that order), shown only when `p.rotation` is present.
- Backups (landed): `Settings::rotation` lives in `settings_model.h`. The YAML writes `rotation: <0-3>  # 0 landscape, 1 portrait, 2 landscape 180, 3 portrait 180` right after `theme`; on read an integer goes through `rotationFromIndex`, and a missing key keeps the current value. Add a round-trip and out-of-range test to `test_backup_yaml`, and add the key to the backup spec §3.

- [ ] **Step 1: Write the failing tests.**
  ```cpp
  void test_rotation_labels_and_order() {
    // rotationLabel(kRotationOrder[i]) for i 0..3 == "Landscape","Landscape 180","Portrait","Portrait 180"
  }
  void test_rotation_from_index_out_of_range() // -1, 4, 255 → 0; 3 → 3
  ```
- [ ] **Step 2: Run** `pio test -e native -f test_touch_input`. Expected: the build fails because `rotation.h` is missing.
- [ ] **Step 3: Implement** `rotation.{h,cpp}`, then the HAL, settings, `ui_task`, `Renderer`, Settings screen and web config changes.
- [ ] **Step 4: Audit portrait.** On `wio_l2_fake` (or by reading each `render` at a 30×20 and a 40×26 grid), every screen must size from `g.cols()`/`g.rows()`. Known risks:
  - LoginForm's 44-character note: wrap it with `wrapText`.
  - Settings' value column at col 11.
  - `Drawer::width` (15 cols) at 30 cols: cap it at `cols / 2`.

  Fix any text that is cut or overlaps.
- [ ] **Step 5: Run** `pio test -e native`, then `scripts/build-upload-monitor.sh -B`. Expected: green, and all four envs build. **M9:** `canRotate()` is false, so the Orientation row and the web field are hidden and `setRotation` is never applied. The extra NVS key `rot` is read as 0.
- [ ] **Step 6: Commit** `feat: selectable orientation on touch boards`.

### Task 12: Docs

**Files:**
- Modify: `docs/USE.md`, `docs/TESTING.md`, `AGENTS.md`

`docs/TESTING.md` may hold uncommitted edits by the user. Stage only your own hunks (`git add -p docs/TESTING.md`) and leave theirs alone.

- [ ] **Step 1: USE.md.** Add a "Touch (Seeed Wio Tracker L2)" section that covers:
  - taps: tap a row to open it, hold a row for its menu, and `⋯` for the screen menu;
  - going back and home: `‹` goes back and holding it goes home; BOOT does the same (tap Back, hold Home); swiping right from the left edge goes back;
  - drag or swipe to scroll;
  - the keyboard: Shift, double-tap caps, `?123`/`#+=`, `▼`, and that enter makes a new line in the Composer Body;
  - Wake: a tap turns the screen off, a tap while dark peeks, a hold wakes;
  - Settings → Orientation and its web-config field.

  Change the title so it no longer names only the M9.
- [ ] **Step 2: TESTING.md.** Under "Open hardware checks (Wio Tracker L2)":
  - replace the temporary button mapping and its "Buttons" item with the new BOOT/Wake mapping;
  - add unticked checks, run on `wio_l2_fake` and then `wio_l2`:
    - tap accuracy at the four corners in each of the four orientations (this confirms Task 1's r1/r3 direction);
    - every screen by touch: Home, Rooms, Room (log tap, drawer, menus), C-Mail, Feed (drag, swipe, load more), Entry, Composer (fields, checkboxes, `⋯` Post/Discard), Settings, Login;
    - the keyboard: all three pages, Shift and caps lock, `⌫`, enter, `▼`, and landscape vs portrait key size;
    - `‹` tap and hold, `⋯`, the edge swipe, and a banner tap;
    - touch never wakes a dark screen;
    - orientation from Settings and from web config, and it survives a reboot.

  Add an M9 regression item: every menu and confirm in Room, Entry, Composer, Feed and Settings behaves as before (Task 4).
- [ ] **Step 3: AGENTS.md.**
  - Add `**Touch UI:** spec docs/superpowers/specs/2026-10-26-cyberspace-touch-design.md, plan docs/superpowers/plans/2026-10-26-cyberspace-touch.md`.
  - Replace "Its touch UI … still being designed; until then its two buttons stand in and `board::readTouch` only feeds a debug log" with a one-line description of the touch layer (`lib/core` gestures/`TapMap`/`Osk`; `ui_task` resolves).
  - Update the native test count in Progress.
- [ ] **Step 4: Commit** `docs: touch UI guide, Wio L2 checks, AGENTS pointer`.
