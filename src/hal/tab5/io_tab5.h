// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#if defined(BOARD_TAB5)

#include <cstdint>

// The two PI4IOE5V6408 expanders on the system I2C (0x43, 0x44). Outputs are
// kept in a shadow, so set() is safe from any task.
namespace tab5::io {

// Resets both and sets them up as M5Unified does, with the 5 V outputs on,
// the amplifier off, the C6 powered and the charger enabled. False when an
// expander didn't answer.
bool begin();
void set(uint8_t addr, uint8_t bit, bool high);
// An input bit; false when the read fails.
bool get(uint8_t addr, uint8_t bit);

}  // namespace tab5::io

#endif
