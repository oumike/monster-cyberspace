#pragma once

#include <cstdint>

// The T-Deck keyboard's ESP32-C3 at I2C 0x55: one byte a read, 0 for none.
namespace tdeck_kb {

bool begin();               // true when 0x55 ACKs (waits up to 500 ms for the C3 to boot)
bool poll(uint8_t& raw);    // at most one read per 10 ms; false on 0 or no data

}  // namespace tdeck_kb
