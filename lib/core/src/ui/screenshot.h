#pragma once

#include <cstddef>
#include <cstdint>

namespace rc {

// Screenshots over the serial console (scripts/screenshot.py). The host sends
// "screenshot\n"; the firmware answers
//   @@SHOT <width> <height>\n <runs> \n@@END <run bytes> <crc32>\n
// where the runs are (count 1..255, RGB565 little-endian) triples, row by row.

// Encodes n pixels as runs into out (room for 3 * n bytes); returns the bytes used.
size_t rle565Row(const uint16_t* px, int n, uint8_t* out);

// zlib's CRC-32; start with 0 and chain.
uint32_t crc32Update(uint32_t crc, const uint8_t* data, size_t n);

// Collects console input into lines and recognises commands.
class SerialCommand {
 public:
  enum Result { None, Screenshot };
  Result feed(char c);

 private:
  static constexpr int kMax = 32;
  char line_[kMax] = {};
  int len_ = 0;
  bool overflow_ = false;
};

}  // namespace rc
