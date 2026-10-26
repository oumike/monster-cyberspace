#pragma once

#include <cstdint>

// The NV3031B on quad SPI, through LovyanGFX's Panel_NV3031B. Landscape
// 320x240; the backlight and touch are driven elsewhere (board_wio_l2.cpp,
// touch_gt911.cpp), so LovyanGFX never touches the I2C bus.
namespace nv3031b {

bool begin();
// Sends rows [y0, y1) of a full-width frame of host-order RGB565.
void pushRows(const uint16_t* frame, int width, int y0, int y1);
void sleep(bool on);

}  // namespace nv3031b
