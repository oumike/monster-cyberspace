#pragma once

#include <cstdint>

// The T-LoRa Pager's XL9555 expander and the lock for its one I2C bus.
namespace pager {

// Wire up, then the rails: keyboard, LoRa (powered, as camillia-mt has it,
// so the unused radio isn't fed through its chip select) and SD on; GPS,
// haptics, NFC and the speaker amp off.
bool ioBegin();
bool ioSet(uint8_t bit, bool high);

void i2cLock();
void i2cUnlock();

// Held across multi-transfer I2C sequences (the UI, network and audio tasks
// share the bus). Recursive, so helpers can nest.
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

}  // namespace pager
