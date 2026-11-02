// SPDX-License-Identifier: GPL-3.0-or-later
// Ported from wadamesh variants/tdisplay_p4/P4Haptic.{h,cpp}.
#pragma once
#if defined(BOARD_TDISPLAY_P4)

// The vibration motor: an LRA driven by an AW86224 (0x58 on Wire1's audio
// route, 20/21), which plays a waveform from its own SRAM, loaded on the
// first buzz. A task of its own plays the buzzes.
namespace tdp4::haptic {

void begin();
// Returns at once. A buzz asked for while one is waiting replaces it; one
// whose bus stays busy is dropped.
void buzz(int ms);

}  // namespace tdp4::haptic

#endif
