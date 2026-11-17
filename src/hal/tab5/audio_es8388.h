// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#if defined(BOARD_TAB5)

#include <cstdint>

// Beeps through the Tab5's ES8388 codec and NS4150B amplifier, on a task of
// their own (as the Tanmatsu's src/hal/tanmatsu/audio_es8156).
namespace tab5::audio {

void begin();
// Returns at once. A beep asked for while one is waiting replaces it.
void beep(int freqHz, int ms);
// The next beep's loudness, as a percent (ui/volume.h).
void setVolume(uint8_t pct);

}  // namespace tab5::audio

#endif
