// SPDX-License-Identifier: GPL-3.0-or-later
// The M5Stack CardputerZero (Raspberry Pi CM0, Linux): the framebuffer,
// keyboard, battery, backlight and beep (spec §5) in this directory's other
// files; the flags in flags_cardputer_zero.cpp.
#if defined(BOARD_CARDPUTER_ZERO)

#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>

#include "cp0_internal.h"
#include "hal/board.h"
#include "linux_hal.h"

namespace cp0 {

namespace {
const auto started = std::chrono::steady_clock::now();
std::atomic<int> level{255};      // backlightLevel's last level
std::atomic<bool> lit{true};      // backlight(on)
std::atomic<bool> asleep{false};  // displaySleep

// Without a backlight node, dark means a black framebuffer.
void apply() {
  const bool dark = !lit || asleep || level == 0;
  if (hasBacklightNode()) {
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

}  // namespace cp0

namespace board {

void begin() {
  if (!cp0::fbOpen()) {
    std::fprintf(stderr, "[boot] no framebuffer: set LV_LINUX_FBDEV_DEVICE\n");
    std::exit(1);
  }
  cp0::powerBegin();
  cp0::audioBegin();
  cp0::inputBegin();
}

int panelWidth() { return cp0::kUiW; }
int panelHeight() { return cp0::kUiH; }
void pushRows(const uint16_t* frame, int y0, int y1) { cp0::fbPush(frame, y0, y1); }

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
  while (!quit() && !cp0::escQuit()) usleep(20 * 1000);
}

void end() {
  cp0::inputEnd();
  cp0::audioEnd();
  cp0::fbClose();
  cp0::powerEnd();
}

}  // namespace linuxhal

#endif
