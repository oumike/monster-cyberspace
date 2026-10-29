#pragma once

#include <cstdint>

// The T-Deck Pro's one I2C bus (keyboard, touch, charger, gauge) and its lock.
namespace tdeck_pro {

void i2cBegin();

// Held across multi-transfer I2C sequences (the UI and web tasks share the
// bus). Recursive, so helpers can nest.
class I2cGuard {
 public:
  I2cGuard();
  ~I2cGuard();
  I2cGuard(const I2cGuard&) = delete;
  I2cGuard& operator=(const I2cGuard&) = delete;
};

// Register helpers on Wire; callers hold the guard.
bool i2cWrite(uint8_t addr, uint8_t reg, uint8_t v);
bool i2cRead(uint8_t addr, uint8_t reg, uint8_t* buf, uint8_t n);
// Raw transfers, for the touch controllers' multi-byte commands.
bool i2cSend(uint8_t addr, const uint8_t* buf, uint8_t n, bool stop = true);
bool i2cRecv(uint8_t addr, uint8_t* buf, uint8_t n);

}  // namespace tdeck_pro
