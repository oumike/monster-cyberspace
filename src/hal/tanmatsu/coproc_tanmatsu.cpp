// SPDX-License-Identifier: GPL-3.0-or-later
// Register access as nicolaielectronics tanmatsu_coprocessor does it.
#if defined(BOARD_TANMATSU)

#include "hal/tanmatsu/coproc_tanmatsu.h"

#include <Wire.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "hal/tanmatsu/pins_tanmatsu.h"

namespace tanmatsu::coproc {
namespace {

SemaphoreHandle_t mutex = nullptr;

struct Guard {
  Guard() { lock(); }
  ~Guard() { unlock(); }
};

}  // namespace

void begin() {
  if (!mutex) mutex = xSemaphoreCreateMutex();
  Wire.begin(I2C_SDA, I2C_SCL, I2C_HZ);
  Wire.setTimeOut(20);  // the keyboard is polled every loop: a stuck read must not stall the UI
}

void lock() {
  if (mutex) xSemaphoreTake(mutex, portMAX_DELAY);
}

void unlock() {
  if (mutex) xSemaphoreGive(mutex);
}

bool write(uint8_t reg, const uint8_t* data, size_t n) {
  Guard g;
  Wire.beginTransmission(COPROC_ADDR);
  Wire.write(reg);
  if (n) Wire.write(data, n);
  return Wire.endTransmission() == 0;
}

bool write8(uint8_t reg, uint8_t v) { return write(reg, &v, 1); }

bool read(uint8_t reg, uint8_t* data, size_t n) {
  Guard g;
  Wire.beginTransmission(COPROC_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(int(COPROC_ADDR), int(n)) != int(n)) return false;
  for (size_t i = 0; i < n; ++i) data[i] = Wire.read();
  return true;
}

}  // namespace tanmatsu::coproc

#endif
