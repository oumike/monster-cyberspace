#pragma once

// Code points beyond 0x20–0x7E and 0xA0–0xFF that the cell fonts must carry.
// scripts/bdf2cells.py parses this file: keep one hex literal per entry.

namespace rc {

constexpr char32_t kExtraGlyphs[] = {
    0x2500, 0x2502, 0x250C, 0x2510, 0x2514, 0x2518, 0x251C, 0x2524, 0x252C,  // ─│┌┐└┘├┤┬
    0x2534, 0x253C,                                                          // ┴┼
    0x25CF, 0x25CC, 0x2715,                                                  // ● ◌ ✕
    0x2582, 0x2584, 0x2586, 0x2580, 0x2588, 0x2592,                          // ▂▄▆▀█▒
    0x00B7, 0x2026, 0x2190, 0x2191, 0x2192, 0x2193, 0x2022, 0x007A,          // ·…←↑→↓•z
    0x25B6, 0x2713,                                                          // ▶ ✓
    0x1F3B2, 0x1F3B1, 0x1F960,                                               // 🎲 🎱 🥠
    0xFFFD,
};

// Glyphs missing from Terminus, drawn with a stand-in's bitmap.
struct GlyphAlias {
  char32_t cp, as;
};
constexpr GlyphAlias kGlyphAliases[] = {
    {0x25CC, 0x25CB},  // ◌ → ○
    {0x2715, 0x00D7},  // ✕ → ×
    {0x1F3B2, 0x002A},  // 🎲 → *
    {0x1F3B1, 0x0038},  // 🎱 → 8
    {0x1F960, 0x0066},  // 🥠 → f
};

}  // namespace rc
