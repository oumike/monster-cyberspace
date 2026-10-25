#include "util/utf8.h"

namespace rc {

namespace {

// Decodes one code point at s[i]; returns the bytes consumed (at least 1).
size_t decodeOne(std::string_view s, size_t i, char32_t& cp) {
  const unsigned char b0 = static_cast<unsigned char>(s[i]);
  if (b0 < 0x80) {
    cp = b0;
    return 1;
  }
  size_t len;
  char32_t min;
  if ((b0 & 0xE0) == 0xC0) {
    len = 2, cp = b0 & 0x1F, min = 0x80;
  } else if ((b0 & 0xF0) == 0xE0) {
    len = 3, cp = b0 & 0x0F, min = 0x800;
  } else if ((b0 & 0xF8) == 0xF0) {
    len = 4, cp = b0 & 0x07, min = 0x10000;
  } else {
    cp = 0xFFFD;
    return 1;
  }
  if (i + len > s.size()) {
    cp = 0xFFFD;
    return 1;
  }
  for (size_t k = 1; k < len; ++k) {
    const unsigned char b = static_cast<unsigned char>(s[i + k]);
    if ((b & 0xC0) != 0x80) {
      cp = 0xFFFD;
      return 1;
    }
    cp = (cp << 6) | (b & 0x3F);
  }
  // Overlong forms, surrogates and values past U+10FFFF are not text.
  if (cp < min || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) {
    cp = 0xFFFD;
    return 1;
  }
  return len;
}

}  // namespace

std::u32string utf8ToU32(std::string_view s) {
  std::u32string out;
  out.reserve(s.size());
  for (size_t i = 0; i < s.size();) {
    char32_t cp;
    i += decodeOne(s, i, cp);
    out.push_back(cp);
  }
  return out;
}

std::string u32ToUtf8(std::u32string_view s) {
  std::string out;
  out.reserve(s.size());
  for (char32_t c : s) {
    if (c > 0x10FFFF || (c >= 0xD800 && c <= 0xDFFF)) c = 0xFFFD;
    if (c < 0x80) {
      out.push_back(static_cast<char>(c));
    } else if (c < 0x800) {
      out.push_back(static_cast<char>(0xC0 | (c >> 6)));
      out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
    } else if (c < 0x10000) {
      out.push_back(static_cast<char>(0xE0 | (c >> 12)));
      out.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
    } else {
      out.push_back(static_cast<char>(0xF0 | (c >> 18)));
      out.push_back(static_cast<char>(0x80 | ((c >> 12) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
    }
  }
  return out;
}

size_t utf8Length(std::string_view s) {
  size_t n = 0;
  for (size_t i = 0; i < s.size(); ++n) {
    char32_t cp;
    i += decodeOne(s, i, cp);
  }
  return n;
}

}  // namespace rc
