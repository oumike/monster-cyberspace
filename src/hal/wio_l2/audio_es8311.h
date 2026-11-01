#pragma once

#include <cstdint>

// Beeps through the ES8311 codec and the speaker amplifier. Tones play on a
// task of their own: waking the amplifier can take a while (250 ms on the Wio).
namespace es8311 {

struct Config {
  uint8_t addr;  // codec I2C address, on Wire
  int mclk, bck, ws, dout;  // mclk < 0: no MCLK line
  uint32_t ampSettleMs;
  void (*amp)(bool on);  // null: no amplifier to switch
  // The shared I2C bus's lock, held around codec transfers.
  void (*lock)();
  void (*unlock)();
  // No MCLK line (the Cardputer ADV): the codec makes its clock from BCLK.
  bool mclkFromBclk = false;
};

void begin(const Config& c);
// Returns at once. A beep asked for while one is waiting replaces it.
void beep(int freqHz, int ms);

}  // namespace es8311
