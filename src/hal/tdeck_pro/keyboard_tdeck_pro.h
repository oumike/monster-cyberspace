#pragma once

#include <cstdint>

#include "input/key_event.h"

// The T-Deck Pro's TCA8418 keyboard, on the shared I2C bus.
namespace tdeck_pro_kb {

bool begin();
bool poll(rc::KeyEvent& key);
// Reads the controller's FIFO into the queue, so keys struck during an
// e-paper refresh (up to a second of waiting on BUSY) aren't lost.
void service();
void setBacklight(uint8_t duty);  // the keyboard light (PWM), 0..255
// millis() of the last key press, so refreshes can wait for typing to pause.
uint32_t lastInputMs();

}  // namespace tdeck_pro_kb
