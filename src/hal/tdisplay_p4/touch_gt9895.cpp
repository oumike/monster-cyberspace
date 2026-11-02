// SPDX-License-Identifier: GPL-3.0-or-later
// Ported from wadamesh variants/tdisplay_p4/Hi8561Touch.cpp (its GT9895 path,
// itself from LilyGO cpp_bus_driver gt9895.cpp); reset timing from SensorLib's
// TouchDrvGT9895.
#if defined(BOARD_TDISPLAY_P4)

#include "hal/tdisplay_p4/touch_gt9895.h"

#include <Arduino.h>
#include <Wire.h>

#include "hal/tdisplay_p4/io_xl9535.h"
#include "hal/tdisplay_p4/pins_tdisplay_p4.h"

namespace tdp4::touch {
namespace {

// The touch report, register 0x00010308, addressed big-endian.
constexpr uint8_t kPointReg[4] = {0x00, 0x01, 0x03, 0x08};
bool present = false;

}  // namespace

bool begin() {
  xl9535::set(EXP_TOUCH_RST, true);
  delay(10);
  xl9535::set(EXP_TOUCH_RST, false);
  delay(30);
  xl9535::set(EXP_TOUCH_RST, true);
  delay(100);
  Wire.beginTransmission(TOUCH_ADDR);
  present = Wire.endTransmission() == 0;
  if (!present) Serial.printf("[tdp4] no GT9895 touch at 0x%02X\n", TOUCH_ADDR);
  return present;
}

// 16 bytes: the event head (buf[2] the finger count), then the first point,
// [id/status][?][x lo][x hi][y lo][y hi][pressure][?].
bool read(int16_t& rx, int16_t& ry) {
  if (!present) return false;
  uint8_t buf[16];
  Wire.beginTransmission(TOUCH_ADDR);
  Wire.write(kPointReg, sizeof kPointReg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(int(TOUCH_ADDR), int(sizeof buf)) != sizeof buf) return false;
  for (uint8_t& b : buf) b = Wire.read();
  const uint8_t fingers = buf[2];
  if (fingers < 1 || fingers > 10) return false;
  rx = int16_t(buf[10] | (buf[11] << 8));
  ry = int16_t(buf[12] | (buf[13] << 8));
  return true;
}

}  // namespace tdp4::touch

#endif
