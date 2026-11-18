// SPDX-License-Identifier: GPL-3.0-or-later
// The ST7123/ST7121 report as M5GFX's Touch_ST7123 and M5's
// M5Tab5-Keyboard-UserDemo (drivers/touch_st7121) read it: started once the
// panel is up, the info read first, MAX_TOUCHES read before each report and
// every slot read (a short read leaves the report latched). The
// probe order as M5GFX's board detect (board_detect/m5/esp32p4.inl).
#if defined(BOARD_TAB5)

#include "hal/tab5/touch_tab5.h"

#include <Arduino.h>
#include <Wire.h>

#include "hal/tab5/pins_tab5.h"
#include "hal/wio_l2/touch_gt911.h"

namespace tab5::touch {
namespace {

constexpr uint16_t kRegFwVersion = 0x0000, kRegMaxCoord = 0x0005, kRegMaxTouches = 0x0009,
                   kRegFwRevision = 0x000C, kRegAdvInfo = 0x0010, kRegReport = 0x0014;
constexpr uint8_t kAdvWithCoord = 0x08;
constexpr int kMaxPoints = 10, kReportBytes = 7;  // M5GFX max_touch_points
constexpr uint32_t kPollMs = 15;

bool st = false;  // the ST controller; else the GT911 when it answered
uint8_t maxTouches = 1;
bool down = false;
int16_t lastX = 0, lastY = 0;
uint32_t lastPollMs = 0;

bool stRead(uint16_t reg, uint8_t* buf, uint8_t n) {
  Wire.beginTransmission(ST_TOUCH_ADDR);
  Wire.write(uint8_t(reg >> 8));
  Wire.write(uint8_t(reg));
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(int(ST_TOUCH_ADDR), int(n)) != n) return false;
  for (uint8_t i = 0; i < n; ++i) buf[i] = Wire.read();
  return true;
}

bool acks(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

void stPoll() {
  down = false;
  uint8_t adv = 0;
  if (!stRead(kRegAdvInfo, &adv, 1) || !(adv & kAdvWithCoord)) return;
  uint8_t m = 0;
  if (!stRead(kRegMaxTouches, &m, 1) || !m) return;
  if (m > kMaxPoints) m = kMaxPoints;
  // The whole block, every slot: read short, the report stays latched.
  uint8_t r[kMaxPoints * kReportBytes] = {};
  if (!stRead(kRegReport, r, uint8_t(m * kReportBytes))) return;
  maxTouches = m;
  for (int i = 0; i < m; ++i) {
    const uint8_t* p = r + i * kReportBytes;
    if (!(p[0] & 0x80)) continue;  // not valid
    lastX = int16_t((p[0] & 0x3F) << 8 | p[1]);
    lastY = int16_t(p[2] << 8 | p[3]);
    down = true;
    return;
  }
}

}  // namespace

panel::Kind detect() {
  pinMode(TOUCH_INT, INPUT);
  for (int i = 0; i < 60; ++i) {
    uint8_t fw = 0;
    if (stRead(kRegFwVersion, &fw, 1)) {
      if (fw == 1 || fw == 3) {
        st = true;
        uint8_t m = 0;
        if (stRead(kRegMaxTouches, &m, 1) && m) maxTouches = m > kMaxPoints ? kMaxPoints : m;
        Serial.printf("[tab5] ST touch FW %u, %u points\n", fw, maxTouches);
        return fw == 1 ? panel::Kind::St7121 : panel::Kind::St7123;
      }
    } else if (acks(GT911_ADDR) || acks(0x5D)) {
      gt911::begin({&Wire, GT911_ADDR, nullptr, nullptr});
      return panel::Kind::Ili9881c;
    }
    delay(10);
  }
  // M5GFX's default when the ST answers with another version, or nothing
  // answers.
  Serial.println("[tab5] touch not identified, assuming ST7123");
  st = acks(ST_TOUCH_ADDR);
  return panel::Kind::St7123;
}

void start() {
  if (!st) return;
  uint8_t rev[4] = {}, info[5] = {};
  if (stRead(kRegFwRevision, rev, 4) && stRead(kRegMaxCoord, info, 5))
    Serial.printf("[tab5] touch rev %u.%u.%u.%u, max %u x %u, %u points\n", rev[0], rev[1], rev[2],
                  rev[3], info[0] << 8 | info[1], info[2] << 8 | info[3], info[4]);
}

bool read(int16_t& x, int16_t& y) {
  if (!st) return gt911::read(x, y);
  const uint32_t now = millis();
  if (now - lastPollMs >= kPollMs) {
    lastPollMs = now;
    stPoll();
  }
  if (!down) return false;
  x = lastX;
  y = lastY;
  return true;
}

}  // namespace tab5::touch

#endif
