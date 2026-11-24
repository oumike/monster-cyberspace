// SPDX-License-Identifier: GPL-3.0-or-later
// The desktop simulator: the CardputerZero's 320x170 panel in an SDL2 window,
// or with MONSTER_DISPLAY=window its desktop window mode (resizable, zoom,
// mouse). The board's flags come from
// src/hal/cardputer_zero/flags_cardputer_zero.cpp.
#if defined(BOARD_LINUX_SIM)

#include <SDL.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <mutex>

#include "desktop_mode.h"
#include "hal/board.h"
#include "input/keymap_cardputer_zero.h"
#include "linux_hal.h"
#include "sdl_keys.h"
#include "sdl_window.h"

namespace {

constexpr int kW = 320;
constexpr int kH = 170;

bool window = false;  // MONSTER_DISPLAY=window

// SDL key events (main thread) become the codes the device's kernel sends
// (spec 2026-11-16 section 5.1) and wait here for pollKey (UI thread).
std::mutex keyMu;
std::deque<rc::ZeroKeyIn> keyQueue;
linuxhal::SdlKeys sdlKeys;   // main thread only
rc::CardputerZeroKeys keys;  // UI thread only
std::atomic<bool> escQuit{false};

void onKey(const SDL_KeyboardEvent& k) {
  rc::ZeroKeyIn in;
  if (!sdlKeys.translate(k, sdlwin::nowMs(), in)) return;
  std::lock_guard<std::mutex> lock(keyMu);
  keyQueue.push_back(in);
}

std::atomic<bool> lit{true};
std::atomic<bool> asleep{false};
std::atomic<uint8_t> level{255};

void applyLevel() { sdlwin::setLevel(lit && !asleep ? level.load() : 0); }

int windowScale() {
  const char* s = std::getenv("MONSTER_SIM_SCALE");
  const int v = s ? std::atoi(s) : 3;
  return v >= 1 && v <= 8 ? v : 3;
}

bool stopRequested() { return escQuit; }

}  // namespace

namespace linuxhal {
bool windowed() { return window; }
}  // namespace linuxhal

namespace board {

void begin() {
  linuxhal::lockOrExit();
  window = linuxhal::chooseDisplayMode(std::getenv("MONSTER_DISPLAY"), nullptr, nullptr) ==
           linuxhal::DisplayMode::Window;
  const int s = windowScale();
  const sdlwin::Options o{window ? "Monster for Cyberspace" : "Monster for Cyberspace (CardputerZero)",
                          window, kW, kH, s, kW * s, kH * s};
  if (!sdlwin::open(o)) std::exit(1);
}

int panelWidth() { return sdlwin::panelWidth(); }
int panelHeight() { return sdlwin::panelHeight(); }
void pushRows(const uint16_t* frame, int y0, int y1) { sdlwin::push(frame, y0, y1); }

void backlight(bool on) {
  lit = on;
  applyLevel();
}

void backlightLevel(uint8_t l) {
  level = l;
  lit = l > 0;
  applyLevel();
}

void displaySleep(bool sleep) {
  asleep = sleep;
  applyLevel();
}

bool pollKey(rc::KeyEvent& out) {
  if (sdlwin::takeKey(out)) return true;
  for (;;) {
    rc::ZeroKeyIn in;
    {
      std::lock_guard<std::mutex> lock(keyMu);
      if (keyQueue.empty()) break;
      in = keyQueue.front();
      keyQueue.pop_front();
    }
    const bool got = keys.feed(in, out);
    keys.takeSystemKey();  // no backlight or volume keys on the host
    if (keys.quitRequested()) escQuit = true;
    if (got) return true;
  }
  const bool got = keys.poll(sdlwin::nowMs(), out);
  if (keys.quitRequested()) escQuit = true;
  return got;
}

bool quitHold(uint32_t& heldMs, uint32_t& totalMs) {
  heldMs = keys.escHeldMs(sdlwin::nowMs());
  totalMs = rc::kZeroQuitHoldMs;
  return heldMs > 0;
}

int batteryPercent() { return 80; }
bool externalPower() { return false; }

void beep(int freqHz, int ms) {
  if (std::getenv("MONSTER_SIM_BEEP")) std::fprintf(stderr, "[sim] beep %d Hz %d ms\n", freqHz, ms);
}

void setVolume(uint8_t) {}

}  // namespace board

namespace linuxhal {

void runMainLoop(bool (*quit)()) { sdlwin::run(quit, stopRequested, onKey, nullptr); }

// MONSTER_SIM_SHOT=<file.bmp>: the last frame, written on exit.
void end() { sdlwin::close(std::getenv("MONSTER_SIM_SHOT")); }

}  // namespace linuxhal

#endif
