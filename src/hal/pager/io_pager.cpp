#if defined(BOARD_PAGER)

#include "hal/pager/io_pager.h"

#include <Arduino.h>
#include <Wire.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "hal/pager/pins_pager.h"

// Rail sequence from camillia-mt's mesh_radio.cpp (pagerPrimeLoRaRail).

namespace pager {
namespace {

constexpr uint8_t kRegOut0 = 0x02, kRegCfg0 = 0x06;  // cfg bit 1 = input

uint8_t addr = 0;
uint8_t out[2] = {0xFF, 0xFF};
uint8_t cfg[2] = {0xFF, 0xFF};
SemaphoreHandle_t busLock = nullptr;

bool commit() {
  return i2cWrite(addr, kRegOut0, out[0]) && i2cWrite(addr, kRegOut0 + 1, out[1]) &&
         i2cWrite(addr, kRegCfg0, cfg[0]) && i2cWrite(addr, kRegCfg0 + 1, cfg[1]);
}

void stage(uint8_t bit, bool high) {
  const uint8_t m = (uint8_t)(1u << (bit & 7));
  cfg[bit >> 3] &= (uint8_t)~m;
  if (high) out[bit >> 3] |= m;
  else out[bit >> 3] &= (uint8_t)~m;
}

void input(uint8_t bit) { cfg[bit >> 3] |= (uint8_t)(1u << (bit & 7)); }

}  // namespace

void i2cLock() {
  if (busLock) xSemaphoreTakeRecursive(busLock, portMAX_DELAY);
}

void i2cUnlock() {
  if (busLock) xSemaphoreGiveRecursive(busLock);
}

I2cGuard::I2cGuard() { i2cLock(); }

I2cGuard::~I2cGuard() { i2cUnlock(); }

bool i2cWrite(uint8_t a, uint8_t reg, uint8_t v) {
  Wire.beginTransmission(a);
  Wire.write(reg);
  Wire.write(v);
  return Wire.endTransmission() == 0;
}

bool i2cRead(uint8_t a, uint8_t reg, uint8_t* buf, uint8_t n) {
  Wire.beginTransmission(a);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(a, n) != n) return false;
  for (uint8_t i = 0; i < n; ++i) buf[i] = Wire.read();
  return true;
}

bool ioBegin() {
  if (!busLock) busLock = xSemaphoreCreateRecursiveMutex();
  Wire.begin(I2C_SDA, I2C_SCL, I2C_HZ);
  I2cGuard g;
  for (uint8_t a = 0x20; a <= 0x27 && !addr; ++a) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) addr = a;
  }
  if (!addr) {
    Serial.println("[pager-io] expander not found");
    return false;
  }
  uint8_t o[2], c[2];
  if (i2cRead(addr, kRegOut0, o, 2) && i2cRead(addr, kRegCfg0, c, 2)) {
    out[0] = o[0], out[1] = o[1], cfg[0] = c[0], cfg[1] = c[1];
  }
  stage(EXP_DRV_EN, false);
  stage(EXP_AMP_EN, false);  // on only while a sound plays
  stage(EXP_KB_RST, true);
  stage(EXP_LORA_EN, true);
  stage(EXP_GPS_EN, false);  // off until the time service turns it on
  stage(EXP_NFC_EN, false);
  stage(EXP_GPS_RST, true);
  stage(EXP_KB_EN, true);
  stage(EXP_GPIO_EN, true);
  stage(EXP_SD_EN, true);
  input(EXP_SD_DET);
  input(EXP_SD_PULLEN);
  const bool ok = commit();
  Serial.printf("[pager-io] 0x%02X %s out=%02X%02X cfg=%02X%02X\n", addr,
                ok ? "ready" : "write failed", out[1], out[0], cfg[1], cfg[0]);
  return ok;
}

bool ioSet(uint8_t bit, bool high) {
  if (!addr) return false;
  I2cGuard g;
  uint8_t o[2] = {out[0], out[1]}, c[2] = {cfg[0], cfg[1]};
  stage(bit, high);
  if (commit()) return true;
  out[0] = o[0], out[1] = o[1], cfg[0] = c[0], cfg[1] = c[1];
  return false;
}

}  // namespace pager

#endif
