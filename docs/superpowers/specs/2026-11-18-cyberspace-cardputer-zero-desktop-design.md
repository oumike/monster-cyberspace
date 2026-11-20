# Monster for Cyberspace in a CardputerZero desktop window — design

Date: 2026-11-18. Status: approved (user, 2026-11-18; default zoom 2x). Builds on the CardputerZero Linux port
(`2026-11-16-cyberspace-cardputer-zero-design.md`).

## 1. Goal

The CardputerZero runs a full Raspberry Pi desktop on HDMI. Started from that
desktop, Monster opens in a normal window there instead of on the small LCD:

- the mouse clicks rows, buttons and menus, and its wheel scrolls lists and
  logs;
- the window can be resized to any size; the UI fills it with more rows and
  columns of text;
- a **Zoom** setting (1x, 2x, 3x) scales every pixel up in whole steps, and
  the rest of the window still fills with text;
- it can be maximised, and **F11** makes it full screen (and back);
- the CardputerZero's own keyboard types into it, as does a USB keyboard.

Started from APPLaunch it stays exactly as today, on the LCD.

## 2. What the device runs (read-only SSH probes, 2026-11-18)

- LightDM auto-logs `mike` into `rpd-labwc` (Raspberry Pi Desktop on the
  labwc Wayland compositor, with Xwayland; X sessions installed too). labwc
  runs even with no monitor, on a stand-in output (`LABWC_FALLBACK_OUTPUT=NOOP-1`).
  Its panel and desktop get `WAYLAND_DISPLAY=wayland-1` and `DISPLAY=:0`.
- APPLaunch is a systemd user service (`APPLaunch.service`) whose
  environment has no `WAYLAND_DISPLAY` or `DISPLAY`. Apps it starts inherit
  that.
- `libsdl2-2.0-0` 2.32.4 and `libinput10` are installed. The desktop's menu
  reads `/usr/share/applications/*.desktop` (M5 ships `fb-mirror.desktop`
  there, a GTK window that mirrors the LCD).
- The keyboard (`tca8418c`, `event3`) is a plain libinput keyboard to labwc,
  with XKB `pc105`/`us` (labwc's environment says `gb`). Its `sym` layer
  codes (26 for `!` …) mean nothing to XKB, so through the desktop the sym
  layer types the wrong characters. HDMI is `card0-HDMI-A-1`.

## 3. Choosing the mode

At start, in `board::begin()` on the `cp0` build:

1. `MONSTER_DISPLAY=lcd` or `=window` forces a mode (for testing).
2. Else, if `WAYLAND_DISPLAY` or `DISPLAY` is set, **window** mode.
3. Else **LCD** mode, as today.

APPLaunch gives no display variables, so its starts stay on the LCD; the
desktop menu gives them, so its starts open a window. One binary, two modes.

Only one Monster runs at a time: a lock on
`~/.config/monster-cyberspace/.lock` (`flock`, non-blocking). A second start
prints `[boot] already running` and exits 0, so the LCD copy and a window
copy never share settings files.

## 4. Window mode

The SDL2 window code the simulator already has, made resizable and shared
(§7). LCD mode keeps `fb_`, `input_`,
`power_` and `audio_` as they are; window mode uses the window and `audio_`.

### 4.1 Window and size

- `SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI`, titled "Monster for
  Cyberspace", first opened at 960x510 (the panel at 3x), with a minimum of
  the panel at the current zoom (320x170 × zoom) so screens never get smaller
  than the LCD's.
- `board::panelWidth()`/`panelHeight()` are the window's drawable size
  divided by the zoom (rounded down). The frame is drawn at that logical size
  and scaled by the zoom with nearest-neighbour filtering into the window, a
  black margin taking any leftover pixels.
- On `SDL_WINDOWEVENT_SIZE_CHANGED` the new size is published; the UI thread
  sees it through a new weak `board::panelSizeChanged()` (default false),
  checked each frame in `ui_task`, and runs the same path a rotation does:
  `lvglResize()` and a relayout. Resizes arrive at most every 50 ms while
  dragging (the newest wins).
- **F11** toggles `SDL_WINDOW_FULLSCREEN_DESKTOP`. F11 is free in the
  CardputerZero key map; it is caught in the HAL and never reaches the app.
  The window manager's maximise works as for any window.
- Closing the window quits, as a long Esc does.

### 4.2 Zoom

- New setting `zoom` (1–3, default 2): NVS `zoom`, backup `zoom`, in device
  Settings as **Zoom** (a 1x / 2x / 3x box) and in web config, shown only
  where the new weak `board::canZoom()` (default false) is true, so only in
  window mode.
- `board::setZoom(z)` re-derives the panel size and reports
  `panelSizeChanged()`; the window's minimum size follows.
- Text size (Compact, Comfortable, Large) still applies on top of zoom;
  window mode returns `hasLargeText()` true (LCD mode keeps false).

### 4.3 Mouse

- `board::hasTouch()` is true in window mode; `readTouch` reports the left
  button held and the pointer's position in panel pixels (window position
  minus the margin, divided by the zoom). LVGL's pointer and the views' row
  hit-testing then work as on touch boards.
- A mouse is not a finger: a new weak `board::pointerIsMouse()` (default
  false) makes `ui_task` skip `input/gestures`' edge swipes, so a drag that
  starts at an edge doesn't go Back.
- The touch status bar is not wanted: `wantHeader()` already leaves it out
  when the board has a keyboard and keys, which it does. Back stays Esc /
  Backspace; the right button sends the Options key (the ⋯ menu).
- The **wheel** sends Up/Down key events, three per notch, which move list
  selections and scroll logs and text as the keys do. Hidden while the
  pointer is idle for 3 s; shown again when it moves.

### 4.4 Keyboard

Two sources, merged into the one key queue `rc::CardputerZeroKeys` reads:

- **The CardputerZero's keyboard**, read as in LCD mode from evdev, so the
  sym layer and modifiers behave exactly as on the LCD. While the window has
  keyboard focus, the reader takes `EVIOCGRAB` on the device, so labwc stops
  seeing those keys and nothing types twice; on focus loss (or quit) it
  releases the grab and the keyboard goes back to the desktop. Without
  focus, its events are dropped.
- **Any other keyboard** (USB, Bluetooth) through SDL key events, mapped to
  the same evdev codes with the simulator's table (`evdevCode`, moved to a
  shared file). SDL events from the grabbed device can't arrive (labwc never
  sees them), so there is no double typing. Physical keys only: no IME or
  text-input events.

The CardputerZero's brightness keys do nothing in window mode (the monitor
has its own); volume and mute still step the beep's volume.

### 4.5 Display power

`backlight`, `backlightLevel` and `displaySleep` dim or black the window's
contents (as the simulator does); the LCD's backlight is left alone. The
screensaver and auto-dim settings still work.

## 5. Packaging

- A second desktop entry, `/usr/share/applications/monster-cyberspace.desktop`
  (`Categories=Network;Chat;`, `Exec=/usr/share/APPLaunch/bin/monster-cyberspace`,
  the same icon installed in `/usr/share/pixmaps/`, as it is 100x100, not a
  hicolor size),
  so the app shows in the desktop's menu under Internet.
  Changed for 2.5.1: the CardputerZero store allows no files in
  `/usr/share/applications/` or `/usr/share/pixmaps/`, so the entry and icon
  install in `/usr/share/monster-cyberspace/` and the app copies the entry to
  `~/.local/share/applications/` at each start (`TryExec` hides it after
  removal).
- The `.deb` gains `Depends: libsdl2-2.0-0 (>= 2.26)` (already installed on
  the device's image).
- The `cp0` build links SDL2 (`pkg_check_modules`), and the Docker image
  (native arm64) gains `libsdl2-dev`.

## 6. Unchanged

LCD mode, the simulator (`MC_BOARD=sim`), every ESP board, and `src/grid_ui/`.
The weak board hooks default to today's behaviour. No new network calls; the
API rules are unchanged.

## 7. Code layout

- `src/hal/linux_sim/sdl_keys.{h,cpp}`: the SDL-to-evdev table (moved out
  of `board_linux_sim.cpp`).
- `src/hal/linux_sim/sdl_window.{h,cpp}`: the window both builds use:
  resizable or fixed, zoom, scaling, mouse, wheel, F11, focus events, dim,
  and the SDL loop on the main thread (`linuxhal::runMainLoop`). Window
  geometry is portable: `lib/core/src/ui/window_fit`.
- `input_cardputer_zero.cpp`: `inputGrab(bool)`, and a flag that drops events
  while unfocused.
- `board_cardputer_zero.cpp`: mode choice, the lock, and dispatch of the
  display calls to `fb_` or the window.
- `flags_cardputer_zero.cpp`: `hasTouch`, `readTouch`, `canZoom`,
  `hasLargeText`, `pointerIsMouse` by mode (the simulator keeps LCD answers,
  or window answers with `MONSTER_DISPLAY=window`, so the simulator can show
  window mode on macOS and Linux hosts).
- Portable: zoom in `AppConfig`, settings, backup YAML and web config in
  `lib/core`/`src/`; `panelSizeChanged`, `canZoom`, `setZoom`,
  `pointerIsMouse` in `board.h` with weak defaults.

## 8. Tests

- `pio test -e native`: the zoom setting's NVS and backup round trip, and its
  web-config field.
- `pio test -e native_lv`: `ScreenHost` and the views after a resize to a
  large size (1920x1080 at 1x) and back to 320x170.
- `linux/tests`: the mode choice (environment combinations), the lock (a
  second instance exits), the pointer mapping (margin and zoom), and the
  wheel's key events.
- The simulator with `MONSTER_DISPLAY=window`: resize, zoom, F11 and the
  mouse by hand.

## 9. Hardware check (docs/TESTING.md)

On a CardputerZero on a monitor: the desktop menu's entry opens a window;
APPLaunch still opens on the LCD; a second start while one runs does
nothing; resizing fills with text; zoom 1x/2x/3x; maximise and F11 full
screen; clicks select, right click opens the menu, the wheel scrolls; the
built-in keyboard types every character including the sym layer, with no
double keys, and goes back to the desktop when the window loses focus; a USB
keyboard and mouse work; screen off and the screensaver dim the window.
