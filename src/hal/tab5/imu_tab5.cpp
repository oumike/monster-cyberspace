// SPDX-License-Identifier: GPL-3.0-or-later
#if defined(BOARD_TAB5)
#include "hal/tab5/imu_tab5.h"

#include <Arduino.h>
#include <Wire.h>

#include <algorithm>

#include "hal/tab5/bmi270_config.h"
#include "hal/tab5/pins_tab5.h"

namespace tab5::imu {
namespace {

constexpr uint8_t kChipId = 0x24;
constexpr uint8_t REG_CHIP_ID = 0x00, REG_ACC_X = 0x0C, REG_INTERNAL_STATUS = 0x21,
                  REG_INIT_CTRL = 0x59, REG_INIT_ADDR_0 = 0x5B, REG_INIT_ADDR_1 = 0x5C,
                  REG_INIT_DATA = 0x5E, REG_PWR_CONF = 0x7C, REG_PWR_CTRL = 0x7D, REG_CMD = 0x7E;
constexpr size_t kChunk = 64;  // well inside Wire's buffer

uint8_t addr = 0;

bool write(uint8_t reg, uint8_t v) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(v);
  return Wire.endTransmission() == 0;
}

bool readRegs(uint8_t reg, uint8_t* out, size_t n) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(int(addr), int(n)) != int(n)) return false;
  for (size_t i = 0; i < n; ++i) out[i] = Wire.read();
  return true;
}

bool probe(uint8_t a) {
  addr = a;
  uint8_t id = 0;
  return readRegs(REG_CHIP_ID, &id, 1) && id == kChipId;
}

bool upload() {
  for (size_t off = 0; off < sizeof(kBmi270Config); off += kChunk) {
    const size_t n = std::min(kChunk, sizeof(kBmi270Config) - off);
    // The address counts 16-bit words: low 4 bits, then the rest.
    if (!write(REG_INIT_ADDR_0, (off / 2) & 0x0F) || !write(REG_INIT_ADDR_1, uint8_t((off / 2) >> 4)))
      return false;
    Wire.beginTransmission(addr);
    Wire.write(REG_INIT_DATA);
    Wire.write(kBmi270Config + off, n);
    if (Wire.endTransmission() != 0) return false;
  }
  return true;
}

}  // namespace

bool begin() {
  if (!probe(IMU_ADDR) && !probe(IMU_ADDR_ALT)) {
    addr = 0;
    return false;
  }
  write(REG_CMD, 0xB6);  // soft reset
  delay(2);
  bool ok = write(REG_PWR_CONF, 0x00);  // advanced power save off, for the upload
  delay(1);
  ok = ok && write(REG_INIT_CTRL, 0x00) && upload() && write(REG_INIT_CTRL, 0x01);
  uint8_t status = 0;
  for (int i = 0; ok && i < 30 && (status & 0x0F) != 1; ++i) {
    delay(5);
    readRegs(REG_INTERNAL_STATUS, &status, 1);
  }
  ok = ok && (status & 0x0F) == 1 && write(REG_PWR_CTRL, 0x04);  // accelerometer on
  if (!ok) {
    Serial.printf("[tab5] imu: init failed (status 0x%02x)\n", status);
    addr = 0;
    return false;
  }
  Serial.printf("[tab5] imu: BMI270 at 0x%02x\n", addr);
  return true;
}

bool read(int16_t raw[3]) {
  if (!addr) return false;
  uint8_t b[6];
  if (!readRegs(REG_ACC_X, b, sizeof b)) return false;
  for (int i = 0; i < 3; ++i) raw[i] = int16_t(b[2 * i] | (b[2 * i + 1] << 8));
  return true;
}

}  // namespace tab5::imu

#endif
