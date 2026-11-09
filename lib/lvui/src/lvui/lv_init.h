#pragma once

#include <cstdint>

namespace lvui {

// Starts LVGL. Call once, from ui_task's thread, after setTickSource.
void init();

// The millisecond clock LVGL's timers run on (millis() on the device).
void setTickSource(uint32_t (*ms)());

// What a failed LV_ASSERT calls (lv_conf.h's LV_ASSERT_HANDLER): the file and
// line. It should not return; the default prints to stderr and aborts.
// nullptr restores the default.
void setAssertHandler(void (*handler)(const char* file, int line));

}  // namespace lvui
