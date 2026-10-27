#pragma once

#include <cstdint>

class TwoWire;

// GT911 capacitive touch, polled over a shared I2C bus.
namespace gt911 {

struct Config {
  TwoWire* bus;
  uint8_t addr;  // tried first; the GT911's other address (0x5D/0x14) next
  void (*lock)();    // may be null
  void (*unlock)();  // may be null
};

bool begin(const Config& c);
bool present();
// True while a finger is down; x, y raw, in the controller's native portrait
// PANEL_NATIVE_W x PANEL_NATIVE_H (rc::touchToFrame maps them to the frame).
bool read(int16_t& x, int16_t& y);

}  // namespace gt911
