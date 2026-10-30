#pragma once

#include <cstdint>

// The T-Display P4's GT9895 touch controller on Wire (camillia-mt's
// Touch_TDisplayP4GT9895). Reset is on the expander.
namespace gt9895 {

bool begin();
bool present();
// True while a finger is down, at (x, y) on the native portrait panel.
bool read(int16_t& x, int16_t& y);

}  // namespace gt9895
