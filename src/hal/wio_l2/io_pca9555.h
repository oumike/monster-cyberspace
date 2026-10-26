#pragma once

#include <cstdint>

// The Wio Tracker L2's PCA9555 expander, and the lock for the shared I2C bus.
namespace wio {

// Wire up, the safe state, then the LCD/touch reset sequence.
bool ioBegin();
bool ioSet(uint8_t bit, bool high);
bool ioWakePressed();

// Held across multi-transfer I2C sequences (the UI and audio tasks share the
// bus). Recursive, so helpers can nest.
class I2cGuard {
 public:
  I2cGuard();
  ~I2cGuard();
  I2cGuard(const I2cGuard&) = delete;
  I2cGuard& operator=(const I2cGuard&) = delete;
};

}  // namespace wio
