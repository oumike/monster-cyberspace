// SPDX-License-Identifier: GPL-3.0-or-later
// The M5Stack CardputerZero (Raspberry Pi CM0, Linux). A stub until the
// device probe (spec §11): the framebuffer, keyboard, battery, backlight and
// beep arrive in the plan's Task 8. The flags are in flags_cardputer_zero.cpp.
#if defined(BOARD_CARDPUTER_ZERO)

#include <unistd.h>

#include "hal/board.h"
#include "linux_hal.h"

namespace board {

void begin() {}

int panelWidth() { return 320; }
int panelHeight() { return 170; }
void pushRows(const uint16_t*, int, int) {}

void backlight(bool) {}
void backlightLevel(uint8_t) {}
void displaySleep(bool) {}

bool pollKey(rc::KeyEvent&) { return false; }

int batteryPercent() { return -1; }
bool externalPower() { return false; }

void beep(int, int) {}
void setVolume(uint8_t) {}

}  // namespace board

namespace linuxhal {

void runMainLoop(bool (*quit)()) {
  while (!quit()) usleep(20 * 1000);
}

void end() {}

}  // namespace linuxhal

#endif
