#pragma once

#include <cstdint>

// Just enough ST7789 (or ST7796) for a landscape panel on a shared SPI bus.
namespace st7789 {

struct Config {
  int sck, mosi, miso, cs, dc, rst;  // rst < 0: no reset line
  uint32_t hz;
  uint8_t madctl;
  bool invert;
  // Where the visible area starts in the controller's memory.
  uint16_t colOffset = 0, rowOffset = 0;
  // Extra commands sent after the reset: {cmd, n, n data bytes}..., then 0xFF.
  const uint8_t* init = nullptr;
};

// Starts the SPI bus on the given pins (a no-op if it's already up). With no
// reset line, only the software reset (0x01) is sent.
void begin(const Config& c);
// Sends rows [y0, y1) of a full-width frame of host-order RGB565.
void pushRows(const uint16_t* frame, int width, int y0, int y1);
void sleep(bool on);

}  // namespace st7789
