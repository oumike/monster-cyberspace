#pragma once

// Fixed-cell bitmap fonts (header-only so native tests can use them).

#include <cstdint>

#include "ui/glyph_set.h"

namespace rc {

// Code points are sorted ascending. Each glyph is h bytes, one per row
// (w <= 8), MSB = leftmost pixel.
struct CellFont {
  uint8_t w, h;
  uint16_t count;
  const uint32_t* codepoints;
  const uint8_t* bitmaps;
};

inline const uint8_t* findGlyph(const CellFont& f, char32_t cp) {
  int lo = 0, hi = static_cast<int>(f.count) - 1;
  while (lo <= hi) {
    int mid = (lo + hi) / 2;
    uint32_t c = f.codepoints[mid];
    if (c == cp) return f.bitmaps + static_cast<size_t>(mid) * f.h;
    if (c < cp) lo = mid + 1;
    else hi = mid - 1;
  }
  return nullptr;
}

// The glyph for cp. Missing code points use their kGlyphAliases stand-in,
// then fall back to U+FFFD, then to '?'; nullptr only if the font has none.
inline const uint8_t* glyphFor(const CellFont& f, char32_t cp) {
  if (const uint8_t* g = findGlyph(f, cp)) return g;
  for (const GlyphAlias& a : kGlyphAliases)
    if (a.cp == cp)
      if (const uint8_t* g = findGlyph(f, a.as)) return g;
  if (const uint8_t* g = findGlyph(f, 0xFFFD)) return g;
  return findGlyph(f, U'?');
}

}  // namespace rc
