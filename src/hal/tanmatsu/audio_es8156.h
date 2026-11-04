#pragma once
#if defined(BOARD_TANMATSU)

#include <cstdint>

// Beeps through the Tanmatsu's ES8156 DAC and its amplifier, on a task of
// their own (from the shared ES8311 tone player, src/hal/wio_l2/audio_es8311).
namespace tanmatsu::audio {

void begin();
// Returns at once. A beep asked for while one is waiting replaces it.
void beep(int freqHz, int ms);
// The next beep's loudness, as a percent (ui/volume.h).
void setVolume(uint8_t pct);

}  // namespace tanmatsu::audio

#endif
