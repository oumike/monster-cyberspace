#if defined(BOARD_TDECK_PRO)

#include "hal/tdeck_pro/io_tdeck_pro.h"

#include <Arduino.h>
#include <Wire.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "hal/tdeck_pro/pins_tdeck_pro.h"

namespace tdeck_pro {
namespace {
SemaphoreHandle_t busLock = nullptr;
}  // namespace

void i2cBegin() {
  if (!busLock) busLock = xSemaphoreCreateRecursiveMutex();
  Wire.begin(I2C_SDA, I2C_SCL, I2C_HZ);
}

I2cGuard::I2cGuard() {
  if (busLock) xSemaphoreTakeRecursive(busLock, portMAX_DELAY);
}

I2cGuard::~I2cGuard() {
  if (busLock) xSemaphoreGiveRecursive(busLock);
}

bool i2cWrite(uint8_t addr, uint8_t reg, uint8_t v) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(v);
  return Wire.endTransmission() == 0;
}

bool i2cRead(uint8_t addr, uint8_t reg, uint8_t* buf, uint8_t n) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  return i2cRecv(addr, buf, n);
}

bool i2cSend(uint8_t addr, const uint8_t* buf, uint8_t n, bool stop) {
  Wire.beginTransmission(addr);
  Wire.write(buf, n);
  return Wire.endTransmission(stop) == 0;
}

bool i2cRecv(uint8_t addr, uint8_t* buf, uint8_t n) {
  if (Wire.requestFrom(addr, n) != n) return false;
  for (uint8_t i = 0; i < n; ++i) buf[i] = uint8_t(Wire.read());
  return true;
}

}  // namespace tdeck_pro

#endif
