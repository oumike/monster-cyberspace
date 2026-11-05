#pragma once
#if defined(BOARD_TANMATSU)

#include <cstddef>
#include <cstdint>

// The Tanmatsu's CH32V203 coprocessor at 0x5F on Wire: a plain register file
// (nicolaielectronics tanmatsu_coprocessor). Every Wire transfer, the codec's
// too, goes through lock(), since the audio task shares the bus.
namespace tanmatsu::coproc {

enum Reg : uint8_t {
  KEYBOARD = 2,            // 9 bytes, one bit per key
  DISPLAY_BACKLIGHT = 11,  // 0-255
  KEYBOARD_BACKLIGHT = 12, // 0-255
  INPUTS = 15,             // bit 0 SD card, bit 1 headphones, bit 2 power button
  OUTPUTS = 16,            // bit 0 amplifier enable
  RADIO = 17,              // the C6: 0 off, 1 bootloader, 2 application
  RTC = 18,                // 4 bytes, seconds, LSB first
  ADC_CONTROL = 108,       // bit 0 trigger, bit 1 continuous
  VBAT = 109,              // 2 bytes, mV, LSB first
  CHARGING = 120,          // bit 0 battery attached, bit 1 USB attached
  LEDS = 127,              // 6 x GRB
  LED_MODE = 145,          // bit 0 automatic
};

void begin();
void lock();
void unlock();
bool write(uint8_t reg, const uint8_t* data, size_t n);
bool write8(uint8_t reg, uint8_t v);
bool read(uint8_t reg, uint8_t* data, size_t n);

}  // namespace tanmatsu::coproc

#endif
