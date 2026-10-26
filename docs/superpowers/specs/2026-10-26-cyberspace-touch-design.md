# Touch UI for touch-only boards (Seeed Wio Tracker L2) — design

Status: approved in conversation 2026-10-26; awaiting written-spec review.

## 1. Goal

Make every existing screen (Home, Rooms and chat, C-Mail, Feed, Entry, Composer,
Settings, Login, Setup) usable by touch on boards with no keyboard, starting
with the Seeed Wio Tracker L2, without forking screens per board. The M9 keeps
working exactly as it does today.

Out of scope: BLE keyboards, haptics, multi-touch, pinch zoom, LVGL.

## 2. Decisions (from the user)

| Topic | Decision |
|---|---|
| Touch model | Direct tap and swipe on the existing screens (no soft d-pad strip) |
| Context menu (the M9's Ctrl) | Long-press an item, **or** a `⋯` button at the right of the status bar |
| Home | Hold BOOT, or hold the status-bar back arrow `‹` |
| Orientation | Selectable in Settings: Landscape, Landscape 180, Portrait, Portrait 180 |
| On-screen keyboard | QWERTY with Shift and a `?123` symbols page, like a phone |

## 3. Architecture

Approach A: a per-frame tap map filled by the shared widgets, plus a pure
gesture recogniser, both in `lib/core` and tested on the host.

### 3.1 HAL (`src/hal/board.h`)

- `bool pollKey(rc::KeyEvent&)` returns a portable key; each board maps its own
  hardware (the M9 keymap moves behind the HAL; done by the bring-up work).
- `bool hasTouch()` and `bool readTouch(int16_t& x, int16_t& y)` (finger down,
  coordinates in the current frame orientation).
- `bool canRotate()` and `void setRotation(uint8_t r)` (0–3). Rotation changes
  the panel scan and the touch transform together; `panelWidth()`/
  `panelHeight()` then report the rotated size (320×240 or 240×320). The M9
  returns `false`/no-op.

### 3.2 Gestures (`lib/core/src/input/gestures.{h,cpp}`)

A pure state machine fed `(down, x, y, ms)` samples. It emits:

- `Tap{x,y}` — up within 10 px of the down point and before 500 ms;
- `LongPress{x,y}` — still down, within 10 px, at 500 ms (emitted once; the
  following up emits nothing);
- `Drag{dy}` — vertical movement past 10 px, emitted per sample while down;
- `Swipe{dir}` — up after more than 40 px of travel in under 300 ms, dominant
  axis decides the direction;
- an edge swipe right that starts within 16 px of the left edge maps to Back.

Thresholds are constants in the header.

### 3.3 Tap map (`lib/core/src/ui/tap_map.{h,cpp}`)

A list of `{rect in pixels, Target}` cleared at the start of every frame and
filled by widgets as they draw. `Target` is one of:

- `Row{list id, index}` — a row in a list or log;
- `Fixed{KeyEvent}` — the `‹` arrow, `⋯`, a keyboard key, a dialog button;
- `Field{field id, cell column, cell row}` — a text field (focus, cursor).

`hit(x, y)` returns the topmost target (later registrations win, so overlays
drawn last take the tap). When `board::hasTouch()` is false, widgets skip
registration.

### 3.4 Resolution to actions

`ui_task` polls touch only while the screen is lit (touch never wakes a dark
screen, the same pocket-safety rule as the M9), feeds `Gestures`, resolves each
gesture against the last frame's `TapMap`, and sends the result through the
existing `handleKey` path:

| Gesture on | Becomes |
|---|---|
| Tap on `Row{i}` | `Activate(i)`: select row *i*, then Select |
| Long-press on `Row{i}` | select row *i*, then `Palette` |
| Tap on `Fixed{k}` | `k` |
| Long-press on `‹` | `OpenHome` |
| Tap on `Field` | focus the field, place the cursor, show the keyboard |
| Drag / Swipe up/down on a list or log | scroll by line / by page |
| Edge swipe right | `Back` |
| Tap on nothing inside an open overlay's dimmed area | `Back` (closes it) |

`rc::Action` gains `Activate` with `KeyEvent::index`. Screens handle it as
"move the selection to `index`, then Select".

### 3.5 Buttons on the Wio L2

- BOOT: tap = Back, hold (600 ms) = Home.
- Wake (expander bit 0): tap = screen off/on; hold = wake. A tap while dark
  shows the screensaver, matching the M9's centre tap (`peeksScreen`).

## 4. Widgets and screens

### 4.1 Shared widgets (`lib/core/src/ui/widgets`)

Each registers its targets while drawing; nothing changes for key input.

- `ListView`, `EntryList`, `Drawer`: one `Row` per visible row.
- `LogView` (chat, Entry): one `Row` per visible message block; drag/swipe
  scroll.
- `Palette`: items are `Row`s; the dimmed area outside closes it.
- `InputLine`, `TextArea`: `Field`; a tap in a `TextArea` moves the cursor to
  that line and column.
- `StatusBar`: on touch boards, draws `‹` at the left (hidden on Home) and `⋯`
  at the right (sends `Palette`), both `Fixed`. The title is shortened to fit.
- `Banner`: a tap dismisses it.

### 4.2 New shared widgets

- **`Overlay`** (`overlay.{h,cpp}`): the one menu and confirm box. Today each
  screen draws its own context menu and "Discard draft? / Log out? / Delete?"
  confirm (already on the deferred-minor list). Screens move to `Overlay`, so
  touch support is added once. Key behaviour (wrap-round, default Cancel) stays
  identical.
- **`Osk`** (`osk.{h,cpp}`): the on-screen keyboard.
  - Docks at the bottom when a focused `InputLine`/`TextArea` wants input; the
    screen above loses those rows (layouts already size from the grid).
  - Pages: letters (Shift is one-shot; a double tap locks caps), `?123`
    (digits and common symbols), and `#+=` (the rest of ASCII, plus the
    glyphs our fonts carry).
  - Bottom row: `?123`, Space, `⌫`, Enter, and `▼` to hide the keyboard (a tap
    on the field brings it back).
  - Keys emit `Char`, `Backspace` and `Select` (Enter), so text widgets need
    no changes. Enter in the Composer Body inserts a newline as Select does.
  - Key size: about 32×36 px in landscape and 24×36 px in portrait.

### 4.3 Screens (`src/screens`)

- All list screens handle `Activate(i)`.
- Screens with menus or confirms switch to `Overlay`.
- Composer: a tap on a field focuses it; a tap on a checkbox toggles it; `⋯`
  opens Post/Discard.
- Login form and Setup: fields use the keyboard. Web config is unchanged.
- Settings: an **Orientation** row (four choices, from an `Overlay`) shown only
  when `board::canRotate()`. It is saved as `cfg.rotation`, also editable in
  web config, applied at once, and restored at boot. Backups include it.
- Any screen that assumes a 320 px width is fixed to size from the grid
  (portrait at small text is about 40 columns).

### 4.4 App (`src/app/ui_task.cpp`)

- Polls touch and runs §3.4.
- On a rotation change: `board::setRotation`, reallocate the renderer frame,
  recompute grid dimensions, and redraw.

## 5. Testing

Native (`pio test -e native`):

- `Gestures`: tap, long-press, drag, swipe in four directions, edge swipe,
  movement that cancels a tap or long-press, and timing boundaries.
- `TapMap`: hit-testing, topmost wins, clearing per frame.
- Touch transform for all four rotations (a pure function in `lib/core`).
- `Osk`: layouts per orientation, page switching, Shift one-shot and lock, and
  the events each key emits.
- Widgets: a rendered `ListView` gives `Row{3}` for a tap on its fourth row;
  `Overlay` key behaviour matches today's menus and confirms.

Builds: `m9`, `m9_fake`, `wio_l2`, `wio_l2_fake` all build; the M9 behaves as
before. Hardware: a Wio L2 checklist in `docs/TESTING.md` (touch accuracy per
rotation, every screen by touch, the keyboard, BOOT and Wake), run mostly on
`wio_l2_fake`.

## 6. Build order

After the Feed plan's Task 12 (the backup feature is also queued after Feed):

1. Hardware bring-up (in progress: HAL, portable `pollKey`, raw touch read).
2. `Gestures`, `TapMap`, touch transform (lib/core, TDD).
3. `Overlay` refactor (M9 only; verified against today's behaviour).
4. Widget tap registration, `Activate`, status-bar `‹` and `⋯`.
5. `Osk`.
6. Orientation (HAL rotation, Settings row, web config, frame resize).
7. Docs: `USE.md` (Wio L2 controls), `TESTING.md`, `AGENTS.md`.
