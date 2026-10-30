#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "input/key_event.h"

namespace rc {

// Screenshots over the serial console (scripts/screenshot.py). The host sends
// "screenshot\n"; the firmware answers
//   @@SHOT <width> <height>\n <runs> \n@@END <run bytes> <crc32>\n
// where the runs are (count 1..255, RGB565 little-endian) triples, row by row.

// Encodes n pixels as runs into out (room for 3 * n bytes); returns the bytes used.
size_t rle565Row(const uint16_t* px, int n, uint8_t* out);

// zlib's CRC-32; start with 0 and chain.
uint32_t crc32Update(uint32_t crc, const uint8_t* data, size_t n);

// Collects console input into lines and recognises commands: "screenshot",
// and for the demo build (RC_FAKE_API) only, "key <name>" (up, select, back,
// home, palette...; see screenshot.cpp) and "type <text>".
class SerialCommand {
 public:
  enum Result { None, Screenshot, Key, Type };
  Result feed(char c);
  KeyEvent key() const { return key_; }    // after Key
  std::string_view text() const { return {line_ + 5, static_cast<size_t>(len_ - 5)}; }  // after Type

 private:
  static constexpr int kMax = 96;
  KeyEvent key_;
  char line_[kMax] = {};
  int len_ = 0;
  bool overflow_ = false;
  bool typed_ = false;
};

}  // namespace rc
