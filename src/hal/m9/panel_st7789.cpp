#if defined(BOARD_M9)

#include "hal/m9/panel_st7789.h"

#include <Arduino.h>
#include <SPI.h>

#include "hal/m9/pins_m9.h"

namespace st7789 {
namespace {

const SPISettings kSettings(m9::TFT_SPI_HZ, MSBFIRST, SPI_MODE0);

void command(uint8_t c, const uint8_t* data = nullptr, size_t n = 0) {
  SPI.beginTransaction(kSettings);
  digitalWrite(m9::TFT_CS, LOW);
  digitalWrite(m9::TFT_DC, LOW);
  SPI.write(c);
  digitalWrite(m9::TFT_DC, HIGH);
  if (n) SPI.writeBytes(data, n);
  digitalWrite(m9::TFT_CS, HIGH);
  SPI.endTransaction();
}

void command1(uint8_t c, uint8_t d) { command(c, &d, 1); }

}  // namespace

void begin() {
  pinMode(m9::TFT_CS, OUTPUT);
  digitalWrite(m9::TFT_CS, HIGH);
  pinMode(m9::TFT_DC, OUTPUT);
  pinMode(m9::TFT_RST, OUTPUT);
  digitalWrite(m9::TFT_RST, HIGH);
  delay(5);
  digitalWrite(m9::TFT_RST, LOW);
  delay(20);
  digitalWrite(m9::TFT_RST, HIGH);
  delay(150);
  command(0x01);  // software reset
  delay(150);
  command(0x11);  // out of sleep
  delay(10);
  command1(0x3A, 0x55);  // 16 bits a pixel
  command1(0x36, 0xA0);  // MADCTL: landscape, keyboard below the screen
  command(0x21);         // inverted, as this panel needs
  command(0x13);         // normal display mode
  delay(10);
  command(0x29);  // on
  delay(10);
}

void pushRows(const uint16_t* frame, int width, int y0, int y1) {
  if (y1 <= y0) return;
  uint8_t ca[4] = {0, 0, (uint8_t)((width - 1) >> 8), (uint8_t)(width - 1)};
  uint8_t ra[4] = {(uint8_t)(y0 >> 8), (uint8_t)y0, (uint8_t)((y1 - 1) >> 8), (uint8_t)(y1 - 1)};
  command(0x2A, ca, 4);
  command(0x2B, ra, 4);
  SPI.beginTransaction(kSettings);
  digitalWrite(m9::TFT_CS, LOW);
  digitalWrite(m9::TFT_DC, LOW);
  SPI.write(0x2C);
  digitalWrite(m9::TFT_DC, HIGH);
  // writePixels swaps to the panel's big-endian order.
  SPI.writePixels(frame + (size_t)y0 * width, (uint32_t)width * (y1 - y0) * 2);
  digitalWrite(m9::TFT_CS, HIGH);
  SPI.endTransaction();
}

void sleep(bool on) {
  command(on ? 0x10 : 0x11);  // SLPIN / SLPOUT
  delay(on ? 5 : 120);
}

}  // namespace st7789

#endif
