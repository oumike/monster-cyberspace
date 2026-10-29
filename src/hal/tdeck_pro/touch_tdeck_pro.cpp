#if defined(BOARD_TDECK_PRO)

#include "hal/tdeck_pro/touch_tdeck_pro.h"

#include <Arduino.h>

#include <algorithm>
#include <initializer_list>

#include "hal/tdeck_pro/io_tdeck_pro.h"
#include "hal/tdeck_pro/pins_tdeck_pro.h"

// Raw I2C for both controllers, from camillia-mt's tdeck_pro_display.h and the
// CSE_CST328 library it uses for the older chip.

namespace tdeck_pro_touch {
namespace {

using namespace tdeck_pro;

enum class Chip : uint8_t { None, Cst328, Cst3530 };
Chip chip = Chip::None;

// Sampled at most this often; between samples the last one stands.
constexpr uint32_t kSampleMs = 15;
// The CST3530 only signals changes; with no report for this long the finger
// is taken as lifted.
constexpr uint32_t kCst3530IdleMs = 120;
uint32_t sampleMs = 0, reportMs = 0;
bool down = false;
int16_t lastX = 0, lastY = 0;

bool send(std::initializer_list<uint8_t> bytes) {
  uint8_t buf[4];
  uint8_t n = 0;
  for (uint8_t b : bytes) buf[n++] = b;
  return i2cSend(TOUCH_ADDR, buf, n);
}

void reset() {
  pinMode(TOUCH_RST, OUTPUT);
  digitalWrite(TOUCH_RST, HIGH);
  delay(20);
  digitalWrite(TOUCH_RST, LOW);
  delay(80);
  digitalWrite(TOUCH_RST, HIGH);
  delay(20);
}

bool probeCst3530() {
  for (int attempt = 0; attempt < 5; ++attempt) {
    uint8_t r[7] = {};
    if (send({0xD0, 0x03, 0x00, 0x00}) && i2cRecv(TOUCH_ADDR, r, sizeof r) && r[2] == 0xCA &&
        r[3] == 0xCA)
      return true;
    send({0xD0, 0x00, 0x04, 0x00});
    delay(50);
  }
  return false;
}

bool initCst3530() {
  bool ok = send({0xD0, 0x00, 0x04, 0x00});
  delay(20);
  ok = send({0xD0, 0x00, 0x04, 0x00}) && ok;
  delay(20);
  ok = send({0xD0, 0x00, 0x00, 0x00}) && ok;
  ok = send({0xD0, 0x00, 0x0C, 0x00}) && ok;
  return send({0xD0, 0x00, 0x01, 0x00}) && ok;
}

// Debug-info mode to read the firmware check (0xCACAxxxx), then normal mode.
bool beginCst328() {
  delay(80);  // TRON after the reset
  for (int i = 0; i < 3; ++i) {
    uint8_t id[4] = {};
    send({0xD1, 0x01});
    if (send({0xD1, 0xFC}) && i2cRecv(TOUCH_ADDR, id, sizeof id) && id[2] == 0xCA &&
        id[3] == 0xCA) {
      send({0xD1, 0x09});
      return true;
    }
  }
  return false;
}

// The first finger's record at 0xD000: state 6 is pressed.
bool readCst328(bool& touched, int16_t& x, int16_t& y) {
  static constexpr uint8_t kReg[] = {0xD0, 0x00};
  uint8_t d[5];
  if (!i2cSend(TOUCH_ADDR, kReg, sizeof kReg, false) || !i2cRecv(TOUCH_ADDR, d, sizeof d))
    return false;
  touched = (d[0] & 0x0F) == 6;
  x = int16_t(d[1] << 4 | d[3] >> 4);
  y = int16_t(d[2] << 4 | (d[3] & 0x0F));
  return true;
}

bool readCst3530(bool& touched, int16_t& x, int16_t& y) {
  uint8_t r[9] = {};
  if (!send({0xD0, 0x07, 0x00, 0x00}) || !i2cRecv(TOUCH_ADDR, r, sizeof r)) return false;
  const uint8_t fingers = r[3] & 0x0F, keys = r[3] >> 4;
  // More records follow when several points are down; only the first is used,
  // so they are read off and dropped.
  if (fingers + keys > 1) {
    uint8_t extra[20];
    i2cRecv(TOUCH_ADDR, extra, uint8_t(std::min<int>(sizeof extra, (fingers + keys - 1) * 5)));
  }
  send({0xD0, 0x00, 0x02, 0xAB});
  touched = false;
  // Touch-key records come before the fingers'; none are expected on this
  // panel, so a report with any is skipped.
  if (!fingers || keys || !(r[8] >> 4)) return true;
  touched = true;
  x = int16_t(r[4] | (r[7] & 0x0F) << 8);
  y = int16_t(r[5] | (r[7] & 0xF0) << 4);
  return true;
}

}  // namespace

bool begin() {
  I2cGuard g;
  reset();
  pinMode(TOUCH_INT, INPUT_PULLUP);
  if (probeCst3530() && initCst3530()) chip = Chip::Cst3530;
  else if (beginCst328()) chip = Chip::Cst328;
  Serial.printf("[touch] %s\n", chip == Chip::Cst3530 ? "CST3530"
                                : chip == Chip::Cst328 ? "CST328"
                                                       : "not found");
  return chip != Chip::None;
}

bool present() { return chip != Chip::None; }

bool read(int16_t& x, int16_t& y) {
  if (chip == Chip::None) return false;
  const uint32_t now = millis();
  if (now - sampleMs >= kSampleMs) {
    sampleMs = now;
    bool touched = false;
    int16_t nx = 0, ny = 0;
    bool ok = false;
    if (chip == Chip::Cst328) {
      I2cGuard g;
      ok = readCst328(touched, nx, ny);
    } else if (digitalRead(TOUCH_INT) == LOW) {
      I2cGuard g;
      ok = readCst3530(touched, nx, ny);
      if (ok) reportMs = now;
    } else if (down && now - reportMs >= kCst3530IdleMs) {
      down = false;
    }
    if (ok) {
      down = touched && nx >= 0 && ny >= 0 && nx < PANEL_W && ny < PANEL_H;
      if (down) {
        lastX = nx;
        lastY = ny;
      }
    }
  }
  if (!down) return false;
  x = lastX;
  y = lastY;
  return true;
}

}  // namespace tdeck_pro_touch

#endif
