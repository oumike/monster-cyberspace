#pragma once

#include <cstdint>

// The CrowPanel 5"'s 800x480 RGB panel, through LovyanGFX's RGB bus. The LCD
// peripheral scans a PSRAM framebuffer out continuously; pushRows copies (and,
// in portrait, turns) the renderer's rows into it.
namespace rgb800 {

bool begin();
// Rows [y0, y1) of a full frame of host-order RGB565, frameW wide, laid out
// for rotation r (quarter turns clockwise from landscape).
void pushRows(const uint16_t* frame, int frameW, int y0, int y1, uint8_t r);
// Blanks the framebuffer (on a rotation change, before the next full frame).
void clear();

}  // namespace rgb800
