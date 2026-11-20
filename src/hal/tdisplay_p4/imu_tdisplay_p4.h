// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#if defined(BOARD_TDISPLAY_P4)

#include <cstdint>

// The T-Display P4's accelerometer on Wire1's 20/21 route (i2c1): an
// ICM20948 on the first boards, a QMI8658 on later ones (LilyGO's
// lilygo_device_driver probes for both). Accelerometer only. Registers as the
// two datasheets.
namespace tdp4::imu {

enum class Chip : uint8_t { None, Icm20948, Qmi8658 };

// Finds and starts whichever is fitted. None when neither answered.
Chip begin();
const char* chipName(Chip c);
// The latest acceleration, raw counts on the chip's own axes. False while
// the bus is busy or the read fails.
bool read(int16_t raw[3]);

}  // namespace tdp4::imu

#endif
