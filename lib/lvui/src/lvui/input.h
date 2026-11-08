#pragma once

#include <cstdint>

#include "input/key_event.h"

namespace lvui {

// A pointer input device read through `read` (true while a finger is down).
void pointerBegin(bool (*read)(int16_t& x, int16_t& y));

// Keys don't go through LVGL: views turn taps into KeyEvents and emit them to
// the sink, which hands them to the screens' onKey like pollKey's.
void setKeySink(void (*sink)(const rc::KeyEvent&));
void emit(const rc::KeyEvent& e);

}  // namespace lvui
