#if defined(BOARD_WIO_L2)

#include "hal/wio_l2/io_pca9555.h"

#include <Arduino.h>
#include <Wire.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "hal/wio_l2/pins_wio_l2.h"

// Bring-up sequence from camillia-mt's wio_tracker_l2_io.cpp, by way of
// platy-aprs's wio_l2_io.cpp.

namespace wio {
namespace {

constexpr uint8_t kRegIn0 = 0x00, kRegOut0 = 0x02, kRegCfg0 = 0x06;

uint8_t out[2] = {0xFF, 0xFF};
uint8_t cfg[2] = {0xFF, 0xFF};
bool ready = false;
SemaphoreHandle_t busLock = nullptr;

bool writeReg(uint8_t reg, uint8_t v) {
  Wire.beginTransmission(EXPANDER_ADDR);
  Wire.write(reg);
  Wire.write(v);
  return Wire.endTransmission() == 0;
}

bool readReg(uint8_t reg, uint8_t& v) {
  Wire.beginTransmission(EXPANDER_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(EXPANDER_ADDR, (uint8_t)1) != 1) return false;
  v = Wire.read();
  return true;
}

bool commit() {
  return writeReg(kRegOut0, out[0]) && writeReg(kRegOut0 + 1, out[1]) &&
         writeReg(kRegCfg0, cfg[0]) && writeReg(kRegCfg0 + 1, cfg[1]);
}

void stage(uint8_t bit, bool high) {
  const uint8_t m = (uint8_t)(1u << (bit & 7));
  cfg[bit >> 3] &= (uint8_t)~m;
  if (high) out[bit >> 3] |= m;
  else out[bit >> 3] &= (uint8_t)~m;
}

void input(uint8_t bit) { cfg[bit >> 3] |= (uint8_t)(1u << (bit & 7)); }

}  // namespace

I2cGuard::I2cGuard() {
  if (busLock) xSemaphoreTakeRecursive(busLock, portMAX_DELAY);
}

I2cGuard::~I2cGuard() {
  if (busLock) xSemaphoreGiveRecursive(busLock);
}

bool ioBegin() {
  ready = false;
  if (!busLock) busLock = xSemaphoreCreateRecursiveMutex();
  pinMode(EXPANDER_INT, INPUT_PULLUP);
  Wire.begin(I2C_SDA, I2C_SCL, I2C_HZ);
  I2cGuard g;
  if (!readReg(kRegOut0, out[0]) || !readReg(kRegOut0 + 1, out[1]) ||
      !readReg(kRegCfg0, cfg[0]) || !readReg(kRegCfg0 + 1, cfg[1])) {
    Serial.println("[wio-io] expander not answering");
    return false;
  }
  input(EXP_WAKE_BUTTON);
  input(EXP_I2C_INT);
  input(EXP_SD_DETECT);
  input(EXP_LCD_CS);
  stage(EXP_TOUCH_INT, false);  // LOW through the touch reset: GT911 at 0x5D
  stage(EXP_LCD_POWER, true);
  stage(EXP_LCD_RST, true);
  stage(EXP_GROVE_POWER, false);
  stage(EXP_TOUCH_RST, false);
  stage(EXP_GNSS_RST, true);
  stage(EXP_USER_LED, false);
  stage(EXP_USB_OTG_EN, false);
  stage(EXP_AUDIO_PA, false);
  stage(EXP_GNSS_POWER, false);  // GPS off until the time service turns it on
  stage(EXP_SD_POWER, false);
  stage(EXP_BATT_SENSE_EN, false);
  bool ok = commit();
  delay(10);
  stage(EXP_GNSS_RST, false);
  ok = commit() && ok;
  delay(40);
  stage(EXP_LCD_RST, false);
  ok = commit() && ok;
  delay(10);
  stage(EXP_LCD_RST, true);
  ok = commit() && ok;
  delay(500);
  stage(EXP_LCD_CS, true);
  ok = commit() && ok;
  delay(10);
  stage(EXP_TOUCH_RST, true);
  ok = commit() && ok;
  delay(60);
  ready = ok;
  Serial.printf("[wio-io] %s out=%02X%02X cfg=%02X%02X\n", ok ? "ready" : "write failed", out[1],
                out[0], cfg[1], cfg[0]);
  return ok;
}

bool ioSet(uint8_t bit, bool high) {
  if (!ready) return false;
  I2cGuard g;
  uint8_t o[2] = {out[0], out[1]}, c[2] = {cfg[0], cfg[1]};
  stage(bit, high);
  if (commit()) return true;
  out[0] = o[0], out[1] = o[1], cfg[0] = c[0], cfg[1] = c[1];
  return false;
}

bool ioWakePressed() {
  if (!ready) return false;
  I2cGuard g;
  uint8_t in = 0xFF;
  if (!readReg(kRegIn0, in)) return false;
  return (in & (1u << EXP_WAKE_BUTTON)) == 0;
}

}  // namespace wio

#endif
