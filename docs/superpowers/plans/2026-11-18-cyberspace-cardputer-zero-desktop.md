# CardputerZero desktop window — implementation plan

Spec: `docs/superpowers/specs/2026-11-18-cyberspace-cardputer-zero-desktop-design.md`.

**Global constraints:** AGENTS.md's hard rules. Portable logic in `lib/core`
with native tests first; hardware and SDL only in `src/hal/cardputer_zero/`
and `src/hal/linux_sim/`, behind `src/hal/board.h`. LCD mode, the ESP boards
and `src/grid_ui/` behave as before. Never call the Cyberspace API; an agent
runs the app only as the simulator's demo build. Commit once per task
(`git commit -s`, a "Tested:" line).

## Task 1: Window geometry (`lib/core/src/ui/window_fit`)

- [x] Tests in `test/test_window_fit`: panel size from a drawable size and a
      zoom (rounded down, never below 1x1), the centred margin, a window
      point to a panel point (inside, in the margin: clamped), zoom clamped
      to 1–3, the minimum window size, wheel notches to Up/Down counts.
- [x] Run `pio test -e native -f test_window_fit`: fails.
- [x] Implement `rc::WindowFit` and helpers; green; whole native suite.
- [x] Commit "Linux: window geometry".

## Task 2: Zoom setting

- [x] `Settings::zoom` (default 2); NVS `zoom`; backup `zoom: 1|2|3`
      (tests in `test_backup_yaml`: round trip, out of range rejected).
- [x] Board hooks with weak defaults in `board.h`/`board_defaults.cpp`:
      `canZoom()` false, `setZoom(uint8_t)`, `panelSizeChanged()` false,
      `pointerIsMouse()` false.
- [x] `ui::setZoom`; Settings' **Zoom** row (1x → 2x → 3x) and web config's
      Zoom row, both only where `canZoom()`; `main`/`ui::start` apply the
      saved zoom.
- [x] `ui_task`: a frame where `panelSizeChanged()` runs `lvglResize()` and
      the relayout; skip edge swipes where `pointerIsMouse()`.
- [x] Native and native_lv green; `pio run -e m9` builds; commit "Zoom
      setting and resizable panels".

## Task 3: Resize to large panels (native_lv)

- [x] `test_lv_*`: `displayResize` to 1920x1080 and back to 316x168 with a
      list screen up: every row inside the panel, no crash; lean build too.
- [x] Fix whatever fails; commit "lvui: survive large resizes".

## Task 4: Window HAL

- [x] Move the simulator's SDL-to-evdev table to
      `src/hal/linux_sim/sdl_keys.{h,cpp}` (shared).
- [x] `src/hal/linux_sim/sdl_window.{h,cpp}`: the SDL window both builds
      use: resizable or fixed, zoom, present with margin, mouse, wheel, F11,
      focus, close, dim; frames from the UI thread under a mutex.
- [x] `cp0`: mode choice (`MONSTER_DISPLAY`, `WAYLAND_DISPLAY`, `DISPLAY`),
      the single-instance lock, window mode dispatch in `board_`; evdev
      `EVIOCGRAB` while focused, drop while unfocused; brightness keys ignored
      in window mode; flags by mode.
- [x] Simulator: fixed LCD-like window as today, or the resizable window with
      `MONSTER_DISPLAY=window`.
- [x] CMake links SDL2 for `cp0`; Docker image adds `libsdl2-dev (native arm64)`.
- [x] `linux/tests`: mode choice and the lock. `ctest --preset sim` green;
      sim builds and its demo runs both ways (`MONSTER_SIM_SHOT`).
- [x] Commit "CardputerZero: desktop window mode".

## Task 5: Packaging

- [x] `packaging/monster-cyberspace-desktop.desktop` to
      `/usr/share/applications/`, icon in `/usr/share/pixmaps` (100x100, not a hicolor size); `Depends` adds
      `libsdl2-2.0-0 (>= 2.26)`.
- [x] `scripts/build-cardputer-zero.sh --deb` builds; `dpkg-deb -c` shows
      both entries. Commit "CardputerZero: desktop menu entry".

## Task 6: Docs

- [ ] `docs/USE.md` (window mode, zoom, mouse, F11, keyboard),
      `docs/BUILD.md`, `docs/TESTING.md` (spec §9 as a hardware check),
      `AGENTS.md`. Commit "Docs: the CardputerZero desktop window".
