#include "util/base64.h"

#include <cstdint>

namespace rc {

namespace {

int sextet(char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52;
  if (c == '+') return 62;
  if (c == '/') return 63;
  return -1;
}

}  // namespace

bool base64Decode(std::string_view in, std::string& out) {
  out.clear();
  uint32_t acc = 0;
  int bits = 0;
  bool padding = false;
  for (char c : in) {
    if (c == '\n' || c == '\r') continue;
    if (c == '=') {
      padding = true;
      continue;
    }
    const int v = sextet(c);
    if (v < 0 || padding) return false;
    acc = (acc << 6) | static_cast<uint32_t>(v);
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      out.push_back(static_cast<char>((acc >> bits) & 0xFF));
    }
  }
  return true;
}

}  // namespace rc
