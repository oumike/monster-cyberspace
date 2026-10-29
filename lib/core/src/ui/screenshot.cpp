#include "ui/screenshot.h"

#include <cstring>

namespace rc {

size_t rle565Row(const uint16_t* px, int n, uint8_t* out) {
  size_t o = 0;
  for (int i = 0; i < n;) {
    const uint16_t c = px[i];
    int run = 1;
    while (i + run < n && run < 255 && px[i + run] == c) ++run;
    out[o++] = static_cast<uint8_t>(run);
    out[o++] = static_cast<uint8_t>(c & 0xFF);
    out[o++] = static_cast<uint8_t>(c >> 8);
    i += run;
  }
  return o;
}

uint32_t crc32Update(uint32_t crc, const uint8_t* data, size_t n) {
  crc = ~crc;
  for (size_t i = 0; i < n; ++i) {
    crc ^= data[i];
    for (int k = 0; k < 8; ++k) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
  }
  return ~crc;
}

SerialCommand::Result SerialCommand::feed(char c) {
  if (c == '\r') return None;
  if (c != '\n') {
    if (len_ < kMax - 1) line_[len_++] = c;
    else overflow_ = true;
    return None;
  }
  line_[len_] = '\0';
  const bool hit = !overflow_ && std::strcmp(line_, "screenshot") == 0;
  len_ = 0;
  overflow_ = false;
  return hit ? Screenshot : None;
}

}  // namespace rc
