#pragma once

#include <cstdint>

// The T-Display P4's main I2C bus, its XL9535 expander, and the ESP32-C6
// Wi-Fi co-processor's power. From camillia-mt's tdisplay_p4_io.cpp.
namespace tdisplay_p4 {

// Wire up, the expander in a safe state (peripherals in reset, SD off).
bool ioBegin();
bool ioSet(uint8_t bit, bool high);
// Power-cycles the ESP32-C6 (Wi-Fi through ESP-Hosted); takes ~1.3 s.
bool resetC6();

// Makes every software restart a full system reset: esp_restart() on the P4
// leaves the MIPI-DSI host half-configured and the panel then stays dark.
// Call before anything else registers a shutdown handler.
void installFullRestart();

// Held across I2C transfers on Wire (UI, web and time tasks share it).
void i2cLock();
void i2cUnlock();
class I2cGuard {
 public:
  I2cGuard() { i2cLock(); }
  ~I2cGuard() { i2cUnlock(); }
  I2cGuard(const I2cGuard&) = delete;
  I2cGuard& operator=(const I2cGuard&) = delete;
};

}  // namespace tdisplay_p4
