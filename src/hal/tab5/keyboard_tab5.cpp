// SPDX-License-Identifier: GPL-3.0-or-later
// Registers from M5Tab5-Keyboard-Internal-FW (user_i2c_reg.h, user_rgb.c).
#if defined(BOARD_TAB5)

#include "hal/tab5/keyboard_tab5.h"

#include <Arduino.h>
#include <Wire.h>

#include "hal/tab5/pins_tab5.h"

namespace tab5::keyboard {
namespace {

constexpr uint8_t kRegIntStatus = 0x01, kRegBrightness = 0x03, kRegMode = 0x10,
                  kRegRgbMode = 0x11, kRegEvent = 0x20, kRegLed0 = 0x60, kRegLed1 = 0x64,
                  kRegVersion = 0xFE;
constexpr uint8_t kModeNormal = 0, kRgbCustom = 1, kNoEvent = 0xFF;
// INT is the trigger; a read this often anyway covers a missed edge.
constexpr uint32_t kIdlePollMs = 50;
constexpr int kMaxEventsPerPoll = 16;

bool fitted = false;
uint32_t lastPollMs = 0;
int drained = 0;
uint16_t ledShown[2] = {0xFFFF, 0xFFFF};

bool write(uint8_t reg, const uint8_t* data, size_t n) {
  Wire1.beginTransmission(KB_ADDR);
  Wire1.write(reg);
  Wire1.write(data, n);
  return Wire1.endTransmission() == 0;
}

bool write8(uint8_t reg, uint8_t v) { return write(reg, &v, 1); }

bool read8(uint8_t reg, uint8_t& v) {
  Wire1.beginTransmission(KB_ADDR);
  Wire1.write(reg);
  if (Wire1.endTransmission(false) != 0) return false;
  if (Wire1.requestFrom(int(KB_ADDR), 1) != 1) return false;
  v = Wire1.read();
  return true;
}

}  // namespace

bool detect() {
  Wire1.begin(KB_SDA, KB_SCL, KB_HZ);
  Wire1.setTimeOut(20);
  uint8_t version = 0;
  if (!read8(kRegVersion, version)) {
    Serial.println("[tab5] no keyboard");
    return false;
  }
  fitted = write8(kRegMode, kModeNormal) && write8(kRegRgbMode, kRgbCustom) &&
           write8(kRegBrightness, 100);
  if (!fitted) {
    Serial.println("[tab5] keyboard setup failed");
    return false;
  }
  // Drop whatever piled up before we looked.
  uint8_t e;
  for (int i = 0; i < 64 && read8(kRegEvent, e) && e != kNoEvent; ++i) {
  }
  write8(kRegIntStatus, 0);
  pinMode(KB_INT, INPUT_PULLUP);
  led(0, 0, 0);
  led(1, 0, 0);
  Serial.printf("[tab5] keyboard fw 0x%02X\n", version);
  return true;
}

bool next(uint8_t& event) {
  if (!fitted) return false;
  const uint32_t now = millis();
  if (drained == 0) {
    if (digitalRead(KB_INT) != LOW && now - lastPollMs < kIdlePollMs) return false;
    lastPollMs = now;
  }
  if (drained < kMaxEventsPerPoll && read8(kRegEvent, event) && event != kNoEvent) {
    ++drained;
    return true;
  }
  if (drained) write8(kRegIntStatus, 0);
  drained = 0;
  return false;
}

void led(int i, uint8_t rgb, uint8_t level) {
  if (!fitted || i < 0 || i > 1) return;
  const uint16_t key = rgb & 7 ? uint16_t((rgb & 7) << 8 | level) : 0;
  if (ledShown[i] == key) return;
  // B, G, R.
  const uint8_t bgr[3] = {uint8_t(rgb & 4 ? level : 0), uint8_t(rgb & 2 ? level : 0),
                          uint8_t(rgb & 1 ? level : 0)};
  if (write(i ? kRegLed1 : kRegLed0, bgr, 3)) ledShown[i] = key;
}

}  // namespace tab5::keyboard

#endif
