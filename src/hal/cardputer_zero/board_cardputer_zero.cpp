// SPDX-License-Identifier: GPL-3.0-or-later
// The M5Stack CardputerZero (Raspberry Pi CM0, Linux): the framebuffer,
// keyboard, battery, backlight and beep (spec §5) in this directory's other
// files; the flags in flags_cardputer_zero.cpp. Started from the desktop
// (a Wayland or X display set) it draws into a window instead of the LCD
// (spec 2026-11-18, src/hal/linux_sim/sdl_window).
#if defined(BOARD_CARDPUTER_ZERO)

#include <unistd.h>

#include <SDL.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>

#include "cp0_internal.h"
#include "hal/board.h"
#include "hal/linux_sim/desktop_mode.h"
#include "hal/linux_sim/sdl_keys.h"
#include "hal/linux_sim/sdl_window.h"
#include "linux_hal.h"

namespace cp0 {

namespace {
const auto started = std::chrono::steady_clock::now();
std::atomic<int> level{255};      // backlightLevel's last level
std::atomic<bool> lit{true};      // backlight(on)
std::atomic<bool> asleep{false};  // displaySleep
bool window = false;               // set once in board::begin()
linuxhal::SdlKeys sdlKeys;         // main thread only

// In a window, dark dims its contents; on the LCD it is the backlight, or
// without a backlight node a black framebuffer.
void apply() {
  const bool dark = !lit || asleep || level == 0;
  if (window) {
    sdlwin::setLevel(dark ? 0 : uint8_t(level.load()));
  } else if (hasBacklightNode()) {
    backlightWrite(dark ? 0 : uint8_t(level.load()));
  } else {
    fbDark(dark);
  }
}
}  // namespace

uint32_t nowMs() {
  return uint32_t(std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::steady_clock::now() - started)
                      .count());
}

int currentBacklight() { return level; }

// Any keyboard but the built-in one (that comes through evdev, grabbed).
void onSdlKey(const SDL_KeyboardEvent& k) {
  rc::ZeroKeyIn in;
  if (sdlKeys.translate(k, nowMs(), in)) queueKey(in.code, in.value, in.ms);
}

}  // namespace cp0

namespace linuxhal {
bool windowed() { return cp0::window; }
}  // namespace linuxhal

namespace board {

void begin() {
  linuxhal::lockOrExit();
  if (linuxhal::chooseDisplayMode(std::getenv("MONSTER_DISPLAY"), std::getenv("WAYLAND_DISPLAY"),
                                  std::getenv("DISPLAY")) == linuxhal::DisplayMode::Window) {
    const sdlwin::Options o{"Monster for Cyberspace", true, cp0::kW, cp0::kH, 1, 960, 510};
    cp0::window = sdlwin::open(o);
    if (!cp0::window) std::fprintf(stderr, "[boot] no window: on the LCD\n");
  }
  std::printf("[boot] display: %s\n", cp0::window ? "window" : "LCD");
  if (!cp0::window && !cp0::fbOpen()) {
    std::fprintf(stderr, "[boot] no framebuffer: set LV_LINUX_FBDEV_DEVICE\n");
    std::exit(1);
  }
  cp0::powerBegin();
  cp0::audioBegin();
  cp0::inputBegin();
}

int panelWidth() { return cp0::window ? sdlwin::panelWidth() : cp0::kUiW; }
int panelHeight() { return cp0::window ? sdlwin::panelHeight() : cp0::kUiH; }
void pushRows(const uint16_t* frame, int y0, int y1) {
  if (cp0::window)
    sdlwin::push(frame, y0, y1);
  else
    cp0::fbPush(frame, y0, y1);
}

void backlight(bool on) {
  cp0::lit = on;
  cp0::apply();
}

void backlightLevel(uint8_t l) {
  cp0::level = l;
  cp0::lit = l > 0;
  cp0::apply();
}

void displaySleep(bool sleep) {
  cp0::asleep = sleep;
  cp0::apply();
}

}  // namespace board

namespace linuxhal {

void runMainLoop(bool (*quit)()) {
  if (cp0::window) {
    sdlwin::run(quit, cp0::escQuit, cp0::onSdlKey, cp0::inputFocus);
    return;
  }
  while (!quit() && !cp0::escQuit()) usleep(20 * 1000);
}

void end() {
  cp0::inputEnd();
  cp0::audioEnd();
  if (cp0::window) {
    sdlwin::close(nullptr);
  } else {
    cp0::fbClose();
    cp0::powerEnd();
  }
}

}  // namespace linuxhal

#endif
