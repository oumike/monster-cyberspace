#pragma once

#include <cstdint>

// GT911 capacitive touch, polled over the shared I2C bus.
namespace gt911 {

bool begin();
bool present();
// True while a finger is down; x, y in the landscape 320x240 frame.
bool read(int16_t& x, int16_t& y);

}  // namespace gt911
