// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#if defined(BOARD_TAB5)

#include <cstdint>

// The Tab5's 720x1280 MIPI-DSI panel from one PSRAM frame buffer (RGB565),
// written by the caller and flushed out of the cache by rows.
namespace tab5::panel {

enum class Kind : uint8_t { St7123, St7121, Ili9881c };

const char* kindName(Kind k);
// The expanders set up first (LCD_RST released). Logs the failing stage and
// returns false.
bool begin(Kind kind);
uint16_t* frame();
// Native rows [y0, y1) written since the last flush.
void flush(int y0, int y1);
void sleep(bool sleep);
// Backlight PWM, 0 off .. 255 full.
void backlight(uint8_t level);

}  // namespace tab5::panel

#endif
