#pragma once

#include <cstdint>

#include "input/key_event.h"

namespace lvui {

// A pointer input device read through `read` (true while a finger is down).
void pointerBegin(bool (*read)(int16_t& x, int16_t& y));
// True once pointerBegin has made the pointer: the board has touch.
bool hasPointer();

// The pointer's press as rc::Gestures tracks one, from every sample read:
// where and when the finger went down, and whether it has since gone past
// rc::kTapSlopPx in either axis. serial counts the presses.
struct Press {
  int16_t x = 0, y = 0;
  uint32_t ms = 0;
  bool moved = false;
  uint32_t serial = 0;
};
const Press& press();
// No keys (touch only): as rc::TouchResolver::setTouchOnly, a tap on a
// scrolling log does nothing and a long press acts on its row.
void setTouchOnly(bool on);
bool touchOnly();

// Keys don't go through LVGL: views turn taps into KeyEvents and emit them to
// the sink, which hands them to the screens' onKey like pollKey's.
void setKeySink(void (*sink)(const rc::KeyEvent&));
void emit(const rc::KeyEvent& e);

}  // namespace lvui
