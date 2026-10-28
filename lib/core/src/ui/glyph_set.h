#pragma once

// Code points the cell fonts must carry beyond Terminus's own glyphs (which
// bdf2cells.py takes whole), drawn through the aliases below.
// scripts/bdf2cells.py parses this file: keep one hex literal per entry.

namespace rc {

constexpr char32_t kExtraGlyphs[] = {
    0x2500, 0x2502, 0x250C, 0x2510, 0x2514, 0x2518, 0x251C, 0x2524, 0x252C,  // ─│┌┐└┘├┤┬
    0x2534, 0x253C,                                                          // ┴┼
    0x25CF, 0x25CC, 0x2715,                                                  // ● ◌ ✕
    0x2582, 0x2584, 0x2586, 0x2580, 0x2588, 0x2592,                          // ▂▄▆▀█▒
    0x00B7, 0x2026, 0x2190, 0x2191, 0x2192, 0x2193, 0x2022, 0x007A,          // ·…←↑→↓•z
    0x25B6, 0x2713,                                                          // ▶ ✓
    0x2039, 0x22EF, 0x25BC, 0x21E7, 0x232B,                                  // ‹ ⋯ ▼ ⇧ ⌫
    0xFFFD,
};

// Glyphs missing from Terminus, drawn with a stand-in's bitmap. bdf2cells.py
// bakes aliases for kExtraGlyphs entries; glyphFor() resolves the rest at run
// time.
struct GlyphAlias {
  char32_t cp, as;
};
constexpr GlyphAlias kGlyphAliases[] = {
    {0x25CC, 0x25CB},  // ◌ → ○
    {0x2715, 0x00D7},  // ✕ → ×
    {0x21A9, 0x2190},  // ↩ → ←
    {0x22EF, 0x2026},  // ⋯ → …
    {0x21E7, 0x2191},  // ⇧ → ↑
    {0x232B, 0x2190},  // ⌫ → ←
    {0x2605, 0x002A},  // ★ → *
};

}  // namespace rc
