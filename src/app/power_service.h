#pragma once

#include <cstdint>

#include "power/power_policy.h"

// Sleep and power off for boards that sleep on battery (the X4; spec
// 2026-10-30-cyberspace-x4-design.md §4.7), carrying out rc::PowerPolicy.
// The UI task owns the screen, so it asks step() each frame and draws the
// last screen before calling sleep() or powerOff(). A no-op elsewhere.
namespace rc::power_service {

void begin();
// A button was pressed (any, even while dark): the wake is now a full one.
void input();
// What to do now.
PowerStep step();
// Deep sleep until the power button or the wake timer. Does not return.
void sleep();
// Cuts the battery. Does not return on the X4.
void powerOff();

}  // namespace rc::power_service
