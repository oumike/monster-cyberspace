#pragma once

#include "input/gestures.h"
#include "input/touch_resolve.h"

namespace rc {

// Vibration pulse lengths, for boards with a motor.
constexpr int kTapBuzzMs = 15, kAlertBuzzMs = 120;

// A tap that hit something (a widget or an on-screen keyboard key) buzzes.
bool tapBuzz(const Gesture& g, const TouchOutput& out);

}  // namespace rc
