#if defined(BOARD_WIO_L2)

#include "hal/wio_l2/touch_gt911.h"

#include <Arduino.h>
#include <Wire.h>

#include "hal/wio_l2/io_pca9555.h"
#include "hal/wio_l2/pins_wio_l2.h"

// Register reads follow LovyanGFX's Touch_GT911. It's read over Wire here, not
// by LovyanGFX, so only one driver owns the I2C peripheral.

namespace gt911 {
namespace {

constexpr uint16_t kRegStatus = 0x814E, kRegPoint1 = 0x814F;
constexpr uint32_t kPollMs = 15;

uint8_t addr = 0;
bool down = false;
int16_t lastX = 0, lastY = 0;
uint32_t lastPollMs = 0;

bool readRegs(uint16_t reg, uint8_t* buf, uint8_t n) {
  Wire.beginTransmission(addr);
  Wire.write((uint8_t)(reg >> 8));
  Wire.write((uint8_t)reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(addr, n) != n) return false;
  for (uint8_t i = 0; i < n; ++i) buf[i] = Wire.read();
  return true;
}

bool writeReg(uint16_t reg, uint8_t v) {
  Wire.beginTransmission(addr);
  Wire.write((uint8_t)(reg >> 8));
  Wire.write((uint8_t)reg);
  Wire.write(v);
  return Wire.endTransmission() == 0;
}

}  // namespace

bool begin() {
  wio::I2cGuard g;
  for (uint8_t a : {wio::TOUCH_ADDR, wio::TOUCH_ADDR_ALT}) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      addr = a;
      break;
    }
  }
  if (!addr) {
    Serial.println("[touch] GT911 not found");
    return false;
  }
  uint8_t id[4] = {};
  readRegs(0x8140, id, 4);
  writeReg(kRegStatus, 0);
  Serial.printf("[touch] GT911 \"%.4s\" at 0x%02X\n", reinterpret_cast<const char*>(id), addr);
  return true;
}

bool present() { return addr != 0; }

bool read(int16_t& x, int16_t& y) {
  if (!addr) return false;
  uint32_t now = millis();
  if (now - lastPollMs >= kPollMs) {
    lastPollMs = now;
    wio::I2cGuard g;
    uint8_t status = 0;
    // Bit 7: a new report is ready. Until it's cleared the GT911 keeps the old
    // one, so a status with bit 7 clear means "no change".
    if (readRegs(kRegStatus, &status, 1) && (status & 0x80)) {
      uint8_t n = status & 0x0F;
      uint8_t p[7];
      if (n > 0 && n <= 5 && readRegs(kRegPoint1, p, sizeof p)) {
        lastX = (int16_t)(p[1] | (p[2] << 8));
        lastY = (int16_t)(p[3] | (p[4] << 8));
        down = true;
      } else if (n == 0) {
        down = false;
      }
      writeReg(kRegStatus, 0);
    }
  }
  x = lastX;
  y = lastY;
  return down;
}

}  // namespace gt911

#endif
