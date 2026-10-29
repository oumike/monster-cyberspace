#pragma once

#include "input/key_event.h"

// The T-LoRa Pager's TCA8418 keyboard, on the shared I2C bus.
namespace pager_kb {

bool begin();  // after the expander has powered it
bool poll(rc::KeyEvent& key);
void setBacklight(uint8_t duty);  // the keyboard light (PWM), 0..255

}  // namespace pager_kb
