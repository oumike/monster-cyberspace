// SPDX-License-Identifier: GPL-3.0-or-later
// The SDL2 window the simulator and the CardputerZero's desktop window mode
// draw into (spec 2026-11-18 §4, §7). Fixed: the panel at a set size,
// stretched over the window, as the simulator always was. Resizable: the
// panel is the window's drawable size over the zoom (ui/window_fit.h), and
// the mouse is a pointer: the left button touches, the right one sends
// Options, the wheel Up/Down; F11 toggles full screen.
//
// SDL runs on the main thread (run); the UI thread pushes frames and asks the
// panel's size, the pointer and the mouse's keys. They meet under a mutex.
#pragma once

#include <cstdint>

#include "input/key_event.h"

struct SDL_KeyboardEvent;

namespace sdlwin {

struct Options {
  const char* title;
  bool resizable;
  int panelW, panelH;  // fixed: the panel; resizable: the LCD's (the minimum at 1x)
  int scale;           // fixed: the window is the panel this many times over
  int firstW, firstH;  // resizable: the window's first size
};

// Main thread. False (with SDL's error printed) if no window opened.
bool open(const Options& o);
// Main thread: pumps events and presents until quit() or stop() is true or
// the window closes. Keys go to onKey (F11 too in a fixed window), focus
// changes to onFocus (either may be null).
void run(bool (*quit)(), bool (*stop)(), void (*onKey)(const SDL_KeyboardEvent&),
         void (*onFocus)(bool focused));
// Main thread: writes the last frame to shotPath (24-bit BMP) if given, then
// closes the window and SDL.
void close(const char* shotPath);

// UI thread.
int panelWidth();
int panelHeight();
void push(const uint16_t* frame, int y0, int y1);
// Resizable: the zoom (Auto, or clamped to 1-3); the panel follows on the next
// takePanelSize.
void setZoom(uint8_t zoom);
// True once the panel's size has changed (a resize or a zoom); panelWidth and
// panelHeight then give the new one. At most one change per 50 ms.
bool takePanelSize();
// The left button held, at its panel position (a press that began on the
// panel holds through the margin, clamped to the edge).
bool readPointer(int16_t& x, int16_t& y);
// Keys from the mouse: Options for a right click, Up/Down for the wheel.
bool takeKey(rc::KeyEvent& out);
// 0 black .. 255 full, for the backlight and display power calls.
void setLevel(uint8_t level);
uint32_t nowMs();

}  // namespace sdlwin
