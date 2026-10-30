#pragma once

#include <cstdint>

class TwoWire;

// Beeps through the ES8311 codec and the speaker amplifier. Tones play on a
// task of their own: waking the amplifier can take a while (250 ms on the Wio).
namespace es8311 {

struct Config {
  uint8_t addr;  // codec I2C address
  int mclk, bck, ws, dout;
  uint32_t ampSettleMs;
  void (*amp)(bool on);
  // The shared I2C bus's lock, held around codec transfers.
  void (*lock)();
  void (*unlock)();
  // The codec's bus, already begun; nullptr is Wire.
  TwoWire* wire = nullptr;
};

void begin(const Config& c);
// Returns at once. A beep asked for while one is waiting replaces it.
void beep(int freqHz, int ms);

}  // namespace es8311
