// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#if defined(BOARD_TAB5)

#include <cstdint>

// The Tab5's BMI270, accelerometer only, on the system I2C. Register use as
// Bosch's datasheet and M5Unified's BMI270_Class.
namespace tab5::imu {

// Resets it, uploads its configuration and starts the accelerometer at
// 100 Hz. False when no BMI270 answered or it didn't start.
bool begin();
// The latest acceleration, raw counts on the chip's own axes.
bool read(int16_t raw[3]);

}  // namespace tab5::imu

#endif
