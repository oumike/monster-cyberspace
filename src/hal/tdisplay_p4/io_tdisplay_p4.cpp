#if defined(BOARD_TDISPLAY_P4)

#include "hal/tdisplay_p4/io_tdisplay_p4.h"

#include <Arduino.h>
#include <Wire.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <hal/wdt_hal.h>

#include "hal/tdisplay_p4/pins_tdisplay_p4.h"

namespace tdisplay_p4 {
namespace {

constexpr uint8_t kRegOut0 = 0x02, kRegPol0 = 0x04, kRegCfg0 = 0x06;

SemaphoreHandle_t busLock = nullptr;
uint8_t out[2] = {0xFF, 0xFF}, cfg[2] = {0xFF, 0xFF};
bool ready = false;

bool readPair(uint8_t reg, uint8_t* v) {
  Wire.beginTransmission(EXPANDER_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(EXPANDER_ADDR, uint8_t(2)) != 2) return false;
  v[0] = Wire.read();
  v[1] = Wire.read();
  return true;
}

bool writePair(uint8_t reg, const uint8_t* v) {
  Wire.beginTransmission(EXPANDER_ADDR);
  Wire.write(reg);
  Wire.write(v[0]);
  Wire.write(v[1]);
  return Wire.endTransmission() == 0;
}

void stage(uint8_t bit, bool high) {
  const uint8_t m = uint8_t(1u << (bit & 7));
  if (high) out[bit >> 3] |= m;
  else out[bit >> 3] &= ~m;
  cfg[bit >> 3] &= ~m;  // an output
}

void input(uint8_t bit) { cfg[bit >> 3] |= uint8_t(1u << (bit & 7)); }

// Levels first, then direction, so no pin glitches on the way.
bool writeState() { return writePair(kRegOut0, out) && writePair(kRegCfg0, cfg); }

// The RTC watchdog's system reset resets the CPU and every peripheral,
// MIPI-DSI included. Registered first, so it runs last (after Wi-Fi's handler).
void fullRestart() {
  wdt_hal_context_t rwdt = RWDT_HAL_CONTEXT_DEFAULT();
  wdt_hal_write_protect_disable(&rwdt);
  wdt_hal_config_stage(&rwdt, WDT_STAGE0, 20000, WDT_STAGE_ACTION_RESET_SYSTEM);
  wdt_hal_enable(&rwdt);
  wdt_hal_write_protect_enable(&rwdt);
  while (true) {
  }
}

}  // namespace

void i2cLock() {
  if (busLock) xSemaphoreTakeRecursive(busLock, portMAX_DELAY);
}

void i2cUnlock() {
  if (busLock) xSemaphoreGiveRecursive(busLock);
}

bool ioBegin() {
  if (ready) return true;
  if (!busLock) busLock = xSemaphoreCreateRecursiveMutex();
  I2cGuard g;
  Wire.begin(I2C_SDA, I2C_SCL, I2C_HZ);
  if (!readPair(kRegOut0, out) || !readPair(kRegCfg0, cfg)) {
    Serial.println("[p4-io] XL9535 not answering");
    return false;
  }
  const uint8_t zero[2] = {0, 0};
  writePair(kRegPol0, zero);
  for (uint8_t b : {EXP_TOUCH_INT, EXP_IMU_INT, EXP_RTC_INT, EXP_C6_WAKE, EXP_RADIO_DIO1}) input(b);
  // camillia-mt's safe state.
  stage(EXP_POWER_3V3, false);
  stage(EXP_ANTENNA_SELECT, true);
  stage(EXP_SCREEN_RST, false);
  stage(EXP_TOUCH_RST, false);
  stage(EXP_ETHERNET_RST, false);
  stage(EXP_AUDIO_POWER, false);
  stage(EXP_USB_PHY_POWER, true);
  stage(EXP_GPS_WAKE, false);
  stage(EXP_C6_EN, true);
  stage(EXP_SD_POWER, true);  // off
  stage(EXP_RADIO_RST, false);
  if (!writeState()) {
    Serial.println("[p4-io] XL9535 write failed");
    return false;
  }
  ready = true;
  return true;
}

bool ioSet(uint8_t bit, bool high) {
  if (!ready) return false;
  I2cGuard g;
  const uint8_t o[2] = {out[0], out[1]}, c[2] = {cfg[0], cfg[1]};
  stage(bit, high);
  if (writeState()) return true;
  out[0] = o[0], out[1] = o[1], cfg[0] = c[0], cfg[1] = c[1];
  return false;
}

bool resetC6() {
  if (!ioSet(EXP_C6_EN, true)) return false;
  delay(100);
  if (!ioSet(EXP_C6_EN, false)) return false;
  delay(100);
  if (!ioSet(EXP_C6_EN, true)) return false;
  delay(1100);
  return true;
}

void installFullRestart() {
  static bool installed = false;
  if (!installed) installed = esp_register_shutdown_handler(fullRestart) == ESP_OK;
}

}  // namespace tdisplay_p4

#endif
