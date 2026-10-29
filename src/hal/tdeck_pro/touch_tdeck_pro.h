#pragma once

#include <cstdint>

// The T-Deck Pro's touch panel: a CST328 or, on later boards, a CST3530,
// told apart at boot. Coordinates are in the panel's 240x320 portrait frame.
namespace tdeck_pro_touch {

bool begin();  // after the I2C bus is up; false when neither answers
bool present();
bool read(int16_t& x, int16_t& y);

}  // namespace tdeck_pro_touch
