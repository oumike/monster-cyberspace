#include <unity.h>

#include <lvgl.h>

#include <cstring>
#include <optional>
#include <string>

#include "cell8x16.h"
#include "emoji_cells.h"
#include "lvui/cell_font.h"
#include "lvui/lv_init.h"
#include "ui/grid.h"
#include "ui/text_width.h"

using rc::TextSize;

void setUp() { lvui::init(); }
void tearDown() {}

namespace {

// Whether pixel (x, y) of cell glyph g is inked, smeared 1 px right when bold.
bool cellPixel(const rc::CellFont& f, const uint8_t* g, int x, int y, bool bold = false) {
  if (rc::glyphPixel(f, g, x, y)) return true;
  return bold && x > 0 && rc::glyphPixel(f, g, x - 1, y);
}

struct Rendered {
  lv_font_glyph_dsc_t dsc{};
  lv_draw_buf_t* buf = nullptr;
  const uint8_t* bits = nullptr;
  uint32_t stride = 0;
  bool inked(int x, int y) const { return bits[y * stride + x] != 0; }
  ~Rendered() {
    if (buf) lv_draw_buf_destroy(buf);
  }
};

void render(const lv_font_t* f, uint32_t cp, Rendered& r) {
  TEST_ASSERT_TRUE(lv_font_get_glyph_dsc(f, &r.dsc, cp, 0));
  r.buf = lv_draw_buf_create(r.dsc.box_w, r.dsc.box_h, LV_COLOR_FORMAT_A8, LV_STRIDE_AUTO);
  TEST_ASSERT_NOT_NULL(r.buf);
  // As LVGL's own fonts, the bitmap comes back as the draw buffer, in A8.
  TEST_ASSERT_EQUAL_PTR(r.buf, lv_font_get_glyph_bitmap(&r.dsc, r.buf));
  r.bits = r.buf->data;
  r.stride = r.buf->header.stride;
}

}  // namespace

void test_ascii_glyph_matches_cell() {
  const lv_font_t* f = lvui::font(TextSize::Comfortable);
  Rendered r;
  render(f, 'A', r);
  TEST_ASSERT_EQUAL(LV_FONT_GLYPH_FORMAT_A1, r.dsc.format);
  TEST_ASSERT_EQUAL(8, r.dsc.adv_w);
  TEST_ASSERT_EQUAL(8, r.dsc.box_w);
  TEST_ASSERT_EQUAL(16, r.dsc.box_h);
  const uint8_t* g = rc::glyphFor(rc::cell8x16, U'A');
  int inked = 0;
  for (int y = 0; y < 16; ++y)
    for (int x = 0; x < 8; ++x) {
      TEST_ASSERT_EQUAL(cellPixel(rc::cell8x16, g, x, y), r.inked(x, y));
      inked += r.inked(x, y);
    }
  TEST_ASSERT_GREATER_THAN(0, inked);
}

void test_missing_uses_alias() {
  // ★ isn't in Terminus; kGlyphAliases draws it as '*'.
  TEST_ASSERT_NULL(rc::findGlyph(rc::cell8x16, 0x2605));
  Rendered r;
  render(lvui::font(TextSize::Comfortable), 0x2605, r);
  TEST_ASSERT_EQUAL(8, r.dsc.adv_w);
  const uint8_t* star = rc::findGlyph(rc::cell8x16, U'*');
  for (int y = 0; y < 16; ++y)
    for (int x = 0; x < 8; ++x) TEST_ASSERT_EQUAL(cellPixel(rc::cell8x16, star, x, y), r.inked(x, y));
}

void test_emoji_two_cells() {
  const char32_t grin = 0x1F600;
  Rendered r;
  render(lvui::font(TextSize::Comfortable), grin, r);
  TEST_ASSERT_EQUAL(16, r.dsc.adv_w);
  TEST_ASSERT_EQUAL(16, r.dsc.box_w);
  const uint8_t* left = rc::findGlyph(rc::emoji8x16, grin);
  const uint8_t* right = rc::findGlyph(rc::emoji8x16, rc::wideTail(grin));
  TEST_ASSERT_NOT_NULL(left);
  TEST_ASSERT_NOT_NULL(right);
  for (int y = 0; y < 16; ++y)
    for (int x = 0; x < 8; ++x) {
      TEST_ASSERT_EQUAL(cellPixel(rc::emoji8x16, left, x, y), r.inked(x, y));
      TEST_ASSERT_EQUAL(cellPixel(rc::emoji8x16, right, x, y), r.inked(8 + x, y));
    }
  // The tail code point and zero-width marks take no room.
  lv_font_glyph_dsc_t d{};
  TEST_ASSERT_TRUE(lv_font_get_glyph_dsc(lvui::font(TextSize::Comfortable), &d, rc::wideTail(grin), 0));
  TEST_ASSERT_EQUAL(0, d.adv_w);
  TEST_ASSERT_TRUE(lv_font_get_glyph_dsc(lvui::font(TextSize::Comfortable), &d, 0xFE0F, 0));
  TEST_ASSERT_EQUAL(0, d.adv_w);
}

void test_text_width_matches_cells() {
  // "a😀b" and a variation selector: 4 cells.
  const char* s = "a\xF0\x9F\x98\x80"
                  "b\xEF\xB8\x8F";
  const std::u32string u = U"a\U0001F600b\uFE0F";
  for (TextSize t : {TextSize::Compact, TextSize::Comfortable, TextSize::Large}) {
    lv_point_t p;
    lv_text_get_size(&p, s, lvui::font(t), 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    TEST_ASSERT_EQUAL(rc::textWidth(u) * rc::cellSize(t).w, p.x);
    TEST_ASSERT_EQUAL(rc::cellSize(t).h, p.y);
  }
}

void test_sizes_follow_cells() {
  for (TextSize t : {TextSize::Compact, TextSize::Comfortable, TextSize::Large}) {
    TEST_ASSERT_EQUAL(rc::cellSize(t).h, lv_font_get_line_height(lvui::font(t)));
    TEST_ASSERT_EQUAL(rc::cellSize(t).w, lv_font_get_glyph_width(lvui::font(t), 'x', 0));
    TEST_ASSERT_EQUAL(rc::bigCellSize(t).h, lv_font_get_line_height(lvui::font(t, true)));
    TEST_ASSERT_EQUAL(rc::bigCellSize(t).w, lv_font_get_glyph_width(lvui::font(t, true), 'x', 0));
  }
  TEST_ASSERT_EQUAL(0, lvui::font(TextSize::Comfortable)->base_line);
}

void test_font_for_size_fixed_wins() {
  TEST_ASSERT_EQUAL_PTR(lvui::font(TextSize::Large), lvui::fontFor(TextSize::Large, std::nullopt));
  TEST_ASSERT_EQUAL_PTR(lvui::font(TextSize::Compact),
                        lvui::fontFor(TextSize::Large, TextSize::Compact));
  TEST_ASSERT_EQUAL_PTR(lvui::font(TextSize::Compact),
                        lvui::fontFor(TextSize::Comfortable, TextSize::Compact));
}

void test_bold_smears_one_pixel() {
  const lv_font_t* f = lvui::font(TextSize::Comfortable, false, true);
  TEST_ASSERT_TRUE(f != lvui::font(TextSize::Comfortable));
  Rendered r;
  render(f, 'l', r);
  TEST_ASSERT_EQUAL(8, r.dsc.adv_w);
  const uint8_t* g = rc::glyphFor(rc::cell8x16, U'l');
  int extra = 0;
  for (int y = 0; y < 16; ++y)
    for (int x = 0; x < 8; ++x) {
      TEST_ASSERT_EQUAL(cellPixel(rc::cell8x16, g, x, y, true), r.inked(x, y));
      extra += r.inked(x, y) && !rc::glyphPixel(rc::cell8x16, g, x, y);
    }
  TEST_ASSERT_GREATER_THAN(0, extra);
}

static uint32_t now = 0;
static uint32_t nowMs() { return now; }
static uint16_t screen[8 * 16];
static void copyFlush(lv_display_t* d, const lv_area_t*, uint8_t* px) {
  memcpy(screen, px, sizeof(screen));
  lv_display_flush_ready(d);
}

void test_label_draws_cell_pixels() {
  lvui::setTickSource(nowMs);
  static uint8_t buf[8 * 16 * 2];
  lv_display_t* disp = lv_display_create(8, 16);
  lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
  lv_display_set_buffers(disp, buf, nullptr, sizeof(buf), LV_DISPLAY_RENDER_MODE_FULL);
  lv_display_set_flush_cb(disp, copyFlush);
  lv_obj_t* scr = lv_display_get_screen_active(disp);
  lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
  lv_obj_t* label = lv_label_create(scr);
  lv_obj_set_style_text_font(label, lvui::font(TextSize::Comfortable), 0);
  lv_obj_set_style_text_color(label, lv_color_white(), 0);
  lv_obj_set_pos(label, 0, 0);
  lv_label_set_text(label, "A");
  now += 100;
  lv_timer_handler();
  const uint8_t* g = rc::glyphFor(rc::cell8x16, U'A');
  for (int y = 0; y < 16; ++y)
    for (int x = 0; x < 8; ++x)
      TEST_ASSERT_EQUAL_HEX16(cellPixel(rc::cell8x16, g, x, y) ? 0xFFFF : 0x0000, screen[y * 8 + x]);
  lv_display_delete(disp);
}

void test_default_font_is_comfortable_cells() {
  TEST_ASSERT_EQUAL_PTR(lvui::font(TextSize::Comfortable), LV_FONT_DEFAULT);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_ascii_glyph_matches_cell);
  RUN_TEST(test_missing_uses_alias);
  RUN_TEST(test_emoji_two_cells);
  RUN_TEST(test_text_width_matches_cells);
  RUN_TEST(test_sizes_follow_cells);
  RUN_TEST(test_font_for_size_fixed_wins);
  RUN_TEST(test_bold_smears_one_pixel);
  RUN_TEST(test_label_draws_cell_pixels);
  RUN_TEST(test_default_font_is_comfortable_cells);
  return UNITY_END();
}
