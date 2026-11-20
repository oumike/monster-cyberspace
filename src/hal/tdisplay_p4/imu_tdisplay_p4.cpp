// SPDX-License-Identifier: GPL-3.0-or-later
#if defined(BOARD_TDISPLAY_P4)
#include "hal/tdisplay_p4/imu_tdisplay_p4.h"

#include <Arduino.h>
#include <Wire.h>

#include "hal/tdisplay_p4/i2c1_tdisplay_p4.h"
#include "hal/tdisplay_p4/pins_tdisplay_p4.h"

namespace tdp4::imu {
namespace {

// ICM20948, user bank 0.
constexpr uint8_t ICM_WHO_AM_I = 0x00, ICM_ID = 0xEA, ICM_PWR_MGMT_1 = 0x06,
                  ICM_PWR_MGMT_2 = 0x07, ICM_ACCEL_XOUT_H = 0x2D, ICM_BANK_SEL = 0x7F;
// QMI8658.
constexpr uint8_t QMI_WHO_AM_I = 0x00, QMI_ID = 0x05, QMI_CTRL1 = 0x02, QMI_CTRL2 = 0x03,
                  QMI_CTRL7 = 0x08, QMI_AX_L = 0x35, QMI_RESET = 0x60;
constexpr uint32_t kBeginWaitMs = 100, kReadWaitMs = 5;

TwoWire* bus = nullptr;  // Wire1 while held
Chip chip = Chip::None;
uint8_t addr = 0;

bool write(uint8_t reg, uint8_t v) {
  bus->beginTransmission(addr);
  bus->write(reg);
  bus->write(v);
  return bus->endTransmission() == 0;
}

bool readRegs(uint8_t reg, uint8_t* out, size_t n) {
  bus->beginTransmission(addr);
  bus->write(reg);
  if (bus->endTransmission(false) != 0) return false;
  if (bus->requestFrom(addr, uint8_t(n)) != n) return false;
  for (size_t i = 0; i < n; ++i) out[i] = bus->read();
  return true;
}

bool idIs(uint8_t a, uint8_t reg, uint8_t id) {
  addr = a;
  uint8_t v = 0;
  return readRegs(reg, &v, 1) && v == id;
}

bool startIcm() {
  if (!write(ICM_PWR_MGMT_1, 0x80)) return false;  // reset
  delay(10);
  write(ICM_BANK_SEL, 0x00);
  // Out of sleep, best clock; the gyroscope off, the accelerometer on
  // (±2 g by default).
  return write(ICM_PWR_MGMT_1, 0x01) && (delay(2), write(ICM_PWR_MGMT_2, 0x07));
}

bool startQmi() {
  write(QMI_RESET, 0xB0);
  delay(20);
  // Register auto-increment, little-endian; ±2 g at about 125 Hz; accelerometer on.
  return write(QMI_CTRL1, 0x40) && write(QMI_CTRL2, 0x06) && write(QMI_CTRL7, 0x01);
}

Chip probe() {
  for (uint8_t a : {ICM_ADDR, ICM_ADDR_ALT}) {
    if (!idIs(a, ICM_WHO_AM_I, ICM_ID)) continue;
    return startIcm() ? Chip::Icm20948 : Chip::None;
  }
  for (uint8_t a : {QMI_ADDR, QMI_ADDR_ALT}) {
    if (!idIs(a, QMI_WHO_AM_I, QMI_ID)) continue;
    return startQmi() ? Chip::Qmi8658 : Chip::None;
  }
  return Chip::None;
}

}  // namespace

Chip begin() {
  bus = i2c1::acquire(i2c1::Audio, kBeginWaitMs);
  if (!bus) return Chip::None;
  write(ICM_BANK_SEL, 0x00);  // harmless where nothing answers
  chip = probe();
  i2c1::release();
  bus = nullptr;
  if (chip == Chip::None) addr = 0;
  return chip;
}

const char* chipName(Chip c) {
  switch (c) {
    case Chip::Icm20948: return "ICM20948";
    case Chip::Qmi8658: return "QMI8658";
    default: return "none";
  }
}

bool read(int16_t raw[3]) {
  if (chip == Chip::None) return false;
  bus = i2c1::acquire(i2c1::Audio, kReadWaitMs);
  if (!bus) return false;
  uint8_t b[6];
  const bool icm = chip == Chip::Icm20948;
  const bool ok = readRegs(icm ? ICM_ACCEL_XOUT_H : QMI_AX_L, b, sizeof b);
  i2c1::release();
  bus = nullptr;
  if (!ok) return false;
  for (int i = 0; i < 3; ++i)
    raw[i] = icm ? int16_t((b[2 * i] << 8) | b[2 * i + 1]) : int16_t(b[2 * i] | (b[2 * i + 1] << 8));
  return true;
}

}  // namespace tdp4::imu

#endif
