// SPDX-License-Identifier: GPL-3.0-or-later
// Ported from wadamesh variants/tdisplay_p4/Xl9535.cpp.
#if defined(BOARD_TDISPLAY_P4)

#include "hal/tdisplay_p4/io_xl9535.h"

#include <Arduino.h>
#include <Wire.h>

#include "hal/tdisplay_p4/pins_tdisplay_p4.h"

namespace tdp4::xl9535 {
namespace {

constexpr uint8_t kRegIn0 = 0x00, kRegOut0 = 0x02, kRegCfg0 = 0x06;  // port 1 is +1; cfg bit 1 = input

bool ok = false;
uint8_t out[2] = {0xFF, 0xFF};  // shadows of the output and config registers
uint8_t cfg[2] = {0xFF, 0xFF};
// The shadows are read, changed and written back from more than one task.
SemaphoreHandle_t lock = nullptr;

void writeReg(uint8_t reg, uint8_t v) {
  Wire.beginTransmission(EXP_ADDR);
  Wire.write(reg);
  Wire.write(v);
  Wire.endTransmission();
}

void setDir(uint8_t bit, bool input) {
  const uint8_t port = bit >> 3, mask = uint8_t(1u << (bit & 7));
  xSemaphoreTake(lock, portMAX_DELAY);
  cfg[port] = input ? (cfg[port] | mask) : (cfg[port] & ~mask);
  writeReg(kRegCfg0 + port, cfg[port]);
  xSemaphoreGive(lock);
}

}  // namespace

bool begin() {
  if (!lock) lock = xSemaphoreCreateMutex();
  Wire.beginTransmission(EXP_ADDR);
  ok = Wire.endTransmission() == 0;
  if (!ok) {
    Serial.printf("[tdp4] XL9535 not found at 0x%02X\n", EXP_ADDR);
    return false;
  }
  out[0] = out[1] = 0xFF;
  cfg[0] = cfg[1] = 0x00;
  writeReg(kRegOut0, out[0]);
  writeReg(kRegOut0 + 1, out[1]);
  writeReg(kRegCfg0, cfg[0]);
  writeReg(kRegCfg0 + 1, cfg[1]);
  return true;
}

void set(uint8_t bit, bool high) {
  if (!ok) return;
  const uint8_t port = bit >> 3, mask = uint8_t(1u << (bit & 7));
  xSemaphoreTake(lock, portMAX_DELAY);
  out[port] = high ? (out[port] | mask) : (out[port] & ~mask);
  writeReg(kRegOut0 + port, out[port]);
  xSemaphoreGive(lock);
}

bool get(uint8_t bit) {
  if (!ok) return false;
  xSemaphoreTake(lock, portMAX_DELAY);
  uint8_t v = 0xFF;
  Wire.beginTransmission(EXP_ADDR);
  Wire.write(uint8_t(kRegIn0 + (bit >> 3)));
  if (Wire.endTransmission(false) == 0 && Wire.requestFrom(int(EXP_ADDR), 1) == 1) v = Wire.read();
  xSemaphoreGive(lock);
  return (v >> (bit & 7)) & 1;
}

void powerOnSequence() {
  if (!ok) return;
  setDir(EXP_TOUCH_INT, true);
  setDir(EXP_IMU_INT, true);
  setDir(EXP_RTC_INT, true);
  setDir(EXP_RADIO_DIO1, true);

  // Replicated exactly from the tested LilyGO/Meck-P4 sequence (wadamesh):
  // the rails are power-cycled with 200 ms settling and end VCCA low, 5V
  // high, 3V3 low. Setting them all high leaves the AMOLED dark.
  set(EXP_GPS_WAKE, true);  // L76K awake
  set(EXP_VCCA, false);
  set(EXP_AUDIO_POWER, true);  // the 5V rail
  delay(200);
  set(EXP_AUDIO_POWER, false);
  delay(200);
  set(EXP_AUDIO_POWER, true);
  set(EXP_3V3, false);
  delay(200);
  set(EXP_3V3, true);
  delay(200);
  set(EXP_3V3, false);
  delay(200);

  set(EXP_SD_POWER, false);  // active low: slot powered
  set(EXP_RADIO_RST, true);
  set(EXP_ANTENNA, true);    // on-board antenna (RF1)
  set(EXP_TOUCH_RST, true);

  // The C6's enable is its reset: pulse it so it boots its ESP-Hosted
  // firmware while the rest of the board comes up.
  set(EXP_C6_EN, true);
  delay(100);
  set(EXP_C6_EN, false);
  delay(100);
  set(EXP_C6_EN, true);
  delay(100);
}

}  // namespace tdp4::xl9535

#endif
