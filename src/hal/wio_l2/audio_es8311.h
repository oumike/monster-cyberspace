#pragma once

// Beeps through the ES8311 codec and the speaker amplifier. Tones play on a
// task of their own: waking the amplifier alone takes 250 ms.
namespace es8311 {

void begin();
// Returns at once. A beep asked for while one is waiting replaces it.
void beep(int freqHz, int ms);

}  // namespace es8311
