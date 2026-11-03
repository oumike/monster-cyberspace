#pragma once

// The T-Deck's I2S speaker amplifier (no codec): 16 kHz, 16-bit mono.
#include <cstdint>

namespace tdeck_audio {

void begin();
// Returns at once; a background task plays a square wave of ms, then silence.
// A new beep replaces one that is playing.
void beep(int freqHz, int ms);
// The next beep's loudness, as a percent (ui/volume.h).
void setVolume(uint8_t pct);

}  // namespace tdeck_audio
