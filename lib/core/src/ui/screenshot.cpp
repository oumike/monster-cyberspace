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

namespace {

struct KeyName {
  const char* name;
  Action action;
};

constexpr KeyName kKeys[] = {
    {"up", Action::Up},           {"down", Action::Down},         {"left", Action::Left},
    {"right", Action::Right},     {"select", Action::Select},     {"back", Action::Back},
    {"home", Action::OpenHome},   {"palette", Action::Palette},   {"options", Action::Options},
    {"backspace", Action::Backspace}, {"symbols", Action::Symbols}, {"emoji", Action::Emoji},
    {"circ", Action::OpenCirc},   {"cmail", Action::OpenCmail},   {"feed", Action::OpenFeed},
    {"screenoff", Action::ScreenOff},
};

}  // namespace

SerialCommand::Result SerialCommand::feed(char c) {
  if (typed_) {
    len_ = 0;
    typed_ = false;
  }
  if (c == '\r') return None;
  if (c != '\n') {
    if (len_ < kMax - 1) line_[len_++] = c;
    else overflow_ = true;
    return None;
  }
  line_[len_] = '\0';
  const bool ok = !overflow_;
  const int len = len_;
  len_ = 0;
  overflow_ = false;
  if (!ok) return None;
  if (std::strcmp(line_, "screenshot") == 0) return Screenshot;
  if (std::strncmp(line_, "type ", 5) == 0) {
    len_ = len;  // text() reads it until the next character arrives
    typed_ = true;
    return Type;
  }
  if (std::strncmp(line_, "key ", 4) == 0) {
    for (const KeyName& k : kKeys) {
      if (std::strcmp(line_ + 4, k.name) == 0) {
        key_ = KeyEvent{};
        key_.action = k.action;
        return Key;
      }
    }
  }
  return None;
}

}  // namespace rc
