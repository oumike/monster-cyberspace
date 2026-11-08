#pragma once

#include <cstdint>

namespace lvui {

// Starts LVGL. Call once, from ui_task's thread, after setTickSource.
void init();

// The millisecond clock LVGL's timers run on (millis() on the device).
void setTickSource(uint32_t (*ms)());

}  // namespace lvui
