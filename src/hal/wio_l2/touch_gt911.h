#pragma once

#include <cstdint>

// GT911 capacitive touch, polled over the shared I2C bus.
namespace gt911 {

bool begin();
bool present();
// True while a finger is down; x, y raw, in the controller's native portrait
// PANEL_NATIVE_W x PANEL_NATIVE_H (rc::touchToFrame maps them to the frame).
bool read(int16_t& x, int16_t& y);

}  // namespace gt911
