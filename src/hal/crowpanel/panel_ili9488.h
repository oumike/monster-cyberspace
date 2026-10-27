#pragma once

#include <cstdint>

// The CrowPanel 3.5"'s ILI9488 on SPI, through LovyanGFX's Panel_ILI9488
// (which sends the RGB666 it needs). Landscape 480x320 at rotation 0.
namespace ili9488 {

bool begin();
// Sends rows [y0, y1) of a full-width frame of host-order RGB565.
void pushRows(const uint16_t* frame, int width, int y0, int y1);
void sleep(bool on);
// Quarter turns clockwise from landscape (ui/rotation.h).
void setRotation(uint8_t r);

}  // namespace ili9488
