#pragma once

#include <cstdint>

#include "input/key_event.h"

namespace rc {

// Maps a byte from the T-Deck keyboard's ESP32-C3 (I2C 0x55): 0x08 Backspace,
// CR or LF Select, printable ASCII a Char, anything else None.
KeyEvent mapTdeckKey(uint8_t raw);

}  // namespace rc
