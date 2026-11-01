#pragma once

#include "input/key_event.h"

// The Cardputer ADV's TCA8418 keyboard, and the lock for the I2C bus it
// shares with the codec.
namespace cardputer_kb {

bool begin();  // starts Wire too
bool poll(rc::KeyEvent& key);

// Held around every transfer on the bus (the UI and audio tasks share it).
// Recursive.
void i2cLock();
void i2cUnlock();

}  // namespace cardputer_kb
