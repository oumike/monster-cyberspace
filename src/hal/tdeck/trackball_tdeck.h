#pragma once

#include "input/key_event.h"

// The T-Deck trackball: four direction pins pulsed as the ball rolls, and a
// click, all active low.
namespace trackball {

void begin();  // INPUT_PULLUP on all five pins; FALLING ISRs count the pulses
// One direction per 45 ms at most, else the click (Select or ScreenOff).
bool poll(rc::KeyEvent& key);

}  // namespace trackball
