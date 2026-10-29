#if defined(BOARD_M9) || defined(BOARD_TDECK) || defined(BOARD_PAGER) || defined(BOARD_MESH_DECK)

#include "hal/m9/panel_st7789.h"

#include <Arduino.h>
#include <SPI.h>

namespace st7789 {
namespace {

Config cfg{};
SPISettings kSettings;

void command(uint8_t c, const uint8_t* data = nullptr, size_t n = 0) {
  SPI.beginTransaction(kSettings);
  digitalWrite(cfg.cs, LOW);
  digitalWrite(cfg.dc, LOW);
  SPI.write(c);
  digitalWrite(cfg.dc, HIGH);
  if (n) SPI.writeBytes(data, n);
  digitalWrite(cfg.cs, HIGH);
  SPI.endTransaction();
}

void command1(uint8_t c, uint8_t d) { command(c, &d, 1); }

}  // namespace

void begin(const Config& c) {
  cfg = c;
  kSettings = SPISettings(cfg.hz, MSBFIRST, SPI_MODE0);
  SPI.begin(cfg.sck, cfg.miso, cfg.mosi);
  pinMode(cfg.cs, OUTPUT);
  digitalWrite(cfg.cs, HIGH);
  pinMode(cfg.dc, OUTPUT);
  if (cfg.rst >= 0) {
    pinMode(cfg.rst, OUTPUT);
    digitalWrite(cfg.rst, HIGH);
    delay(5);
    digitalWrite(cfg.rst, LOW);
    delay(20);
    digitalWrite(cfg.rst, HIGH);
    delay(150);
  }
  command(0x01);  // software reset
  delay(150);
  for (const uint8_t* p = cfg.init; p && *p != 0xFF; p += 2 + p[1]) command(p[0], p + 2, p[1]);
  command(0x11);  // out of sleep
  delay(10);
  command1(0x3A, 0x55);  // 16 bits a pixel
  command1(0x36, cfg.madctl);
  command(cfg.invert ? 0x21 : 0x20);
  command(0x13);         // normal display mode
  delay(10);
  command(0x29);  // on
  delay(10);
}

void pushRows(const uint16_t* frame, int width, int y0, int y1) {
  if (y1 <= y0) return;
  const int x0 = cfg.colOffset, x1 = cfg.colOffset + width - 1;
  const int r0 = cfg.rowOffset + y0, r1 = cfg.rowOffset + y1 - 1;
  uint8_t ca[4] = {(uint8_t)(x0 >> 8), (uint8_t)x0, (uint8_t)(x1 >> 8), (uint8_t)x1};
  uint8_t ra[4] = {(uint8_t)(r0 >> 8), (uint8_t)r0, (uint8_t)(r1 >> 8), (uint8_t)r1};
  command(0x2A, ca, 4);
  command(0x2B, ra, 4);
  SPI.beginTransaction(kSettings);
  digitalWrite(cfg.cs, LOW);
  digitalWrite(cfg.dc, LOW);
  SPI.write(0x2C);
  digitalWrite(cfg.dc, HIGH);
  // writePixels swaps to the panel's big-endian order.
  SPI.writePixels(frame + (size_t)y0 * width, (uint32_t)width * (y1 - y0) * 2);
  digitalWrite(cfg.cs, HIGH);
  SPI.endTransaction();
}

void sleep(bool on) {
  command(on ? 0x10 : 0x11);  // SLPIN / SLPOUT
  delay(on ? 5 : 120);
}

}  // namespace st7789

#endif
