#pragma once

#include <cstdint>

#include "input/key_event.h"

namespace rc {

// Maps a raw STC8H key code. Early controllers (hw 0x03, fw 0x10) report
// Back as 0x86; production ones use 0x87.
KeyEvent mapM9Key(uint8_t raw, bool earlyController);

}  // namespace rc
