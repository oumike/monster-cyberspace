#pragma once

#include <cstdint>

// Just enough ST7789 for a landscape 320x240 panel on a shared SPI bus.
namespace st7789 {

void begin();
// Sends rows [y0, y1) of a full-width frame of host-order RGB565.
void pushRows(const uint16_t* frame, int width, int y0, int y1);
void sleep(bool on);

}  // namespace st7789
