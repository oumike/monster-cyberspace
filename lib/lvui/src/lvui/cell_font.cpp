#include "lvui/cell_font.h"

#include "cell10x18.h"
#include "cell12x24.h"
#include "cell6x12.h"
#include "cell8x14.h"
#include "cell8x16.h"
#include "feature_flags.h"
#include "ui/text_width.h"
#if RC_FEAT_EMOJI
#include "emoji_cells.h"
#else
#include "ui/emoji_placeholder.h"
#endif

namespace lvui {

namespace {

struct Face {
  const rc::CellFont* cells;
  bool bold;
};

// Index: size (6x12, 8x14, 8x16, 10x18, 12x24) * 2 + bold.
constexpr Face kFaces[] = {
    {&rc::cell6x12, false},  {&rc::cell6x12, true},  {&rc::cell8x14, false},
    {&rc::cell8x14, true},   {&rc::cell8x16, false}, {&rc::cell8x16, true},
    {&rc::cell10x18, false}, {&rc::cell10x18, true}, {&rc::cell12x24, false},
    {&rc::cell12x24, true},
};

const Face& faceOf(const lv_font_t* f) { return *static_cast<const Face*>(f->dsc); }

bool glyphDsc(const lv_font_t* font, lv_font_glyph_dsc_t* dsc, uint32_t letter, uint32_t) {
  const rc::CellFont& c = *faceOf(font).cells;
  const char32_t cp = static_cast<char32_t>(letter);
  // A tail only follows its emoji in a grid cell; the emoji's glyph has both halves.
  const int cells = rc::isWideTail(cp) ? 0 : rc::cellWidth(cp);
  dsc->resolved_font = font;
  dsc->format = LV_FONT_GLYPH_FORMAT_A1;
  dsc->adv_w = static_cast<uint16_t>(cells * c.w);
#if RC_FEAT_EMOJI
  dsc->box_w = dsc->adv_w;
#else
  // The placeholder in the left cell; the right one blank.
  dsc->box_w = cells ? c.w : 0;
#endif
  dsc->box_h = cells ? c.h : 0;
  dsc->ofs_x = 0;
  dsc->ofs_y = 0;
  dsc->stride = 0;
  dsc->is_placeholder = 0;
  dsc->gid.index = letter;
  return true;
}

// Row y of glyph g, left-aligned in 16 bits as the grid renderer reads it,
// cut to the cell's width.
uint16_t rowBits(const rc::CellFont& f, const uint8_t* g, int y, bool bold) {
  if (!g) return 0;
  uint16_t bits = rc::rowBytes(f) == 1 ? g[y] << 8 : (g[y * 2] << 8) | g[y * 2 + 1];
  if (bold) bits |= bits >> 1;
  return bits & static_cast<uint16_t>(0xFFFF << (16 - f.w));
}

// Expanded to A8 into LVGL's draw buffer, which is returned: the software
// renderer takes A8.
const void* glyphBitmap(lv_font_glyph_dsc_t* dsc, lv_draw_buf_t* buf) {
  if (!buf || dsc->box_w == 0 || dsc->box_h == 0) return nullptr;
  const Face& face = faceOf(dsc->resolved_font);
  const rc::CellFont& c = *face.cells;
  const char32_t cp = static_cast<char32_t>(dsc->gid.index);
  const uint8_t* halves[2] = {nullptr, nullptr};
#if RC_FEAT_EMOJI
  halves[0] = rc::cellGlyph(c, cp);
  if (dsc->box_w > c.w) halves[1] = rc::cellGlyph(c, rc::wideTail(cp));
#else
  halves[0] = rc::glyphFor(c, rc::emojiStandIn(cp));
#endif
  uint8_t* out = buf->data;
  const uint32_t stride = buf->header.stride;
  for (int y = 0; y < dsc->box_h; ++y) {
    const uint32_t bits = (static_cast<uint32_t>(rowBits(c, halves[0], y, face.bold)) << 16) |
                          (static_cast<uint32_t>(rowBits(c, halves[1], y, face.bold)) << (16 - c.w));
    uint8_t* line = out + y * stride;
    for (int x = 0; x < dsc->box_w; ++x) line[x] = (bits & (0x80000000u >> x)) ? 0xFF : 0x00;
  }
  return buf;
}

constexpr lv_font_t makeFont(const Face& face) {
  lv_font_t f{};
  f.get_glyph_dsc = glyphDsc;
  f.get_glyph_bitmap = glyphBitmap;
  f.release_glyph = nullptr;
  f.line_height = face.cells->h;
  f.base_line = 0;
  f.subpx = LV_FONT_SUBPX_NONE;
  f.kerning = LV_FONT_KERNING_NONE;
  f.static_bitmap = 0;
  f.underline_position = 1;  // the cell's bottom pixel line
  f.underline_thickness = 1;
  f.dsc = &face;
  f.fallback = nullptr;
  f.user_data = nullptr;
  return f;
}

const lv_font_t kFonts[] = {
    makeFont(kFaces[0]), makeFont(kFaces[1]), makeFont(kFaces[2]), makeFont(kFaces[3]),
    makeFont(kFaces[4]), makeFont(kFaces[5]), makeFont(kFaces[6]), makeFont(kFaces[7]),
    makeFont(kFaces[8]), makeFont(kFaces[9]),
};

// The kFaces size for a text size, as rc::cellSize and rc::bigCellSize.
int sizeIndex(rc::TextSize s, bool big) {
  switch (s) {
    case rc::TextSize::Compact: return big ? 1 : 0;
    case rc::TextSize::Large: return big ? 4 : 3;
    default: return big ? 3 : 2;
  }
}

}  // namespace

const lv_font_t* font(rc::TextSize s, bool big, bool bold) {
  if (s == rc::TextSize::Comfortable && !big && !bold) return &lv_font_cell8x16;
  return &kFonts[sizeIndex(s, big) * 2 + (bold ? 1 : 0)];
}

const lv_font_t* fontFor(rc::TextSize saved, std::optional<rc::TextSize> fixed) {
  return font(fixed.value_or(saved));
}

}  // namespace lvui

extern "C" const lv_font_t lv_font_cell8x16 = lvui::makeFont(lvui::kFaces[4]);
