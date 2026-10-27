#pragma once

#include <cstdint>

// Just enough ST7789 for a landscape 320x240 panel on a shared SPI bus.
namespace st7789 {

struct Config {
  int sck, mosi, miso, cs, dc, rst;  // rst < 0: no reset line
  uint32_t hz;
  uint8_t madctl;
  bool invert;
};

// Starts the SPI bus on the given pins (a no-op if it's already up). With no
// reset line, only the software reset (0x01) is sent.
void begin(const Config& c);
// Sends rows [y0, y1) of a full-width frame of host-order RGB565.
void pushRows(const uint16_t* frame, int width, int y0, int y1);
void sleep(bool on);

}  // namespace st7789
