#pragma once

#include <cstdint>

// The M9's STC8H keyboard controller on its own I2C bus.
namespace stc8h {

// Releases the USB pads, starts the bus and finds the controller.
bool begin();
// Reads the pending key when INT is high, or every 20 ms regardless, because
// early controllers may never raise INT. False when there is no key.
bool poll(uint8_t& raw);
bool early();
// How bright the controller lights the keys after a press (register 0x02);
// 0 switches that light off until a power cycle.
void setBacklight(uint8_t level);

}  // namespace stc8h
