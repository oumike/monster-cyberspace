#if defined(BOARD_M9)

#include "hal/m9/keyboard_stc8h.h"

#include <Arduino.h>
#include <Wire.h>
#include <driver/gpio.h>

#include "hal/m9/pins_m9.h"
#include "soc/usb_serial_jtag_reg.h"

namespace stc8h {
namespace {

uint8_t addr = 0;
uint8_t keyReg = m9::KB_REG_KEY;
bool isEarly = false;
uint32_t lastPollMs = 0;

bool readReg(uint8_t reg, uint8_t& value) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission() != 0 || Wire.requestFrom(addr, (uint8_t)1) != 1) return false;
  value = Wire.read();
  return true;
}

// GPIO19/20 are the S3's USB pads, held by the USB-Serial-JTAG peripheral
// (with a pull-up on D+) from reset. The keyboard bus reuses 20/21, so the
// claim has to go before the controller answers. Same as camillia-mt's
// m9ReleaseUsbPads().
void releaseUsbPads() {
  REG_CLR_BIT(USB_SERIAL_JTAG_CONF0_REG, USB_SERIAL_JTAG_USB_PAD_ENABLE);
  REG_CLR_BIT(USB_SERIAL_JTAG_CONF0_REG, USB_SERIAL_JTAG_DP_PULLUP);
  gpio_reset_pin(GPIO_NUM_19);
  gpio_reset_pin(GPIO_NUM_20);
  delay(5);
}

}  // namespace

bool begin() {
  pinMode(m9::KB_INT, INPUT_PULLDOWN);
  releaseUsbPads();
  Wire.begin(m9::KB_SDA, m9::KB_SCL, 100000);
  delay(50);
  for (uint8_t a : m9::KB_ADDRS) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      addr = a;
      break;
    }
  }
  if (!addr) {
    Serial.println("[kb] no keyboard found");
    return false;
  }
  uint8_t hw = 0xFF, fw = 0xFF;
  isEarly = readReg(m9::KB_REG_HW, hw) && readReg(m9::KB_REG_FW, fw) && hw == 0x03 && fw == 0x10;
  keyReg = isEarly ? m9::KB_REG_KEY_EARLY : m9::KB_REG_KEY;
  Serial.printf("[kb] keyboard at 0x%02X, hw 0x%02X fw 0x%02X, key register 0x%02X\n", addr, hw,
                fw, keyReg);
  return true;
}

bool poll(uint8_t& raw) {
  if (!addr) return false;
  uint32_t now = millis();
  bool intHigh = digitalRead(m9::KB_INT) == HIGH;
  if (!intHigh && now - lastPollMs < 20) return false;
  lastPollMs = now;
  uint8_t code;
  if (!readReg(keyReg, code)) return false;
  if (code == 0x00 || code == 0xFF) return false;
  if (isEarly && code == 0x88) return false;  // early units' "invalid"
  raw = code;
  return true;
}

bool early() { return isEarly; }

}  // namespace stc8h

#endif
