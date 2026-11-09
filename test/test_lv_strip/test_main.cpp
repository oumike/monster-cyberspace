#include <unity.h>

#include <lvgl.h>

#include <vector>

#include "lvui/display.h"
#include "lvui/lv_init.h"

// Strip mode (boards without PSRAM): LVGL draws a few full-width rows at a
// time, each pushed as a frame shifted back so pushRows finds row y at
// frame + y * w, as it would in a whole frame.
namespace {

constexpr int kW = 40, kH = 30, kStrip = 4;

uint32_t now = 0;
uint32_t nowMs() { return now; }

int curW = kW;
uint16_t panel[kH][kW];  // what pushRows was given, row by row
struct Band {
  int y0, y1;
};
std::vector<Band> bands;
void recordRows(const uint16_t* frame, int y0, int y1) {
  bands.push_back({y0, y1});
  for (int y = y0; y < y1; ++y)
    for (int x = 0; x < curW; ++x) panel[y][x] = frame[static_cast<size_t>(y) * curW + x];
}

lv_display_t* disp = nullptr;

void refresh() {
  now += 50;
  lv_refr_now(disp);
}

uint16_t rgb565(uint32_t hex) { return lv_color_to_u16(lv_color_hex(hex)); }

}  // namespace

void setUp() {
  lvui::setTickSource(nowMs);
  lvui::init();
  if (!disp) disp = lvui::displayBegin({kW, kH, recordRows, kStrip});
  lv_obj_clean(lv_screen_active());
  lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(lv_screen_active(), LV_OPA_COVER, 0);
  refresh();
  bands.clear();
}
void tearDown() {}

void test_strip_has_no_frame() {
  TEST_ASSERT_NOT_NULL(disp);
  TEST_ASSERT_NULL(lvui::frame());
}

void test_strip_pushes_full_width_rows_a_strip_at_a_time() {
  lv_obj_t* box = lv_obj_create(lv_screen_active());
  lv_obj_remove_style_all(box);
  lv_obj_set_style_bg_color(box, lv_color_hex(0xFF0000), 0);
  lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
  lv_obj_set_pos(box, 10, 5);
  lv_obj_set_size(box, 6, 11);
  refresh();

  TEST_ASSERT_FALSE(bands.empty());
  int lo = kH, hi = 0;
  for (const Band& b : bands) {
    TEST_ASSERT_LESS_OR_EQUAL(kStrip, b.y1 - b.y0);
    if (b.y0 < lo) lo = b.y0;
    if (b.y1 > hi) hi = b.y1;
  }
  TEST_ASSERT_LESS_OR_EQUAL(5, lo);
  TEST_ASSERT_GREATER_OR_EQUAL(16, hi);

  const uint16_t red = rgb565(0xFF0000), black = rgb565(0x000000);
  for (int y = 5; y < 16; ++y) {
    // The whole row went out: the box, and the screen beside it to both edges.
    TEST_ASSERT_EQUAL_HEX16(black, panel[y][0]);
    TEST_ASSERT_EQUAL_HEX16(black, panel[y][9]);
    TEST_ASSERT_EQUAL_HEX16(red, panel[y][10]);
    TEST_ASSERT_EQUAL_HEX16(red, panel[y][15]);
    TEST_ASSERT_EQUAL_HEX16(black, panel[y][16]);
    TEST_ASSERT_EQUAL_HEX16(black, panel[y][kW - 1]);
  }
}

void test_strip_follows_a_resize() {
  curW = kW / 2;
  lvui::displayResize(curW, kH);
  TEST_ASSERT_EQUAL(curW, lv_display_get_horizontal_resolution(disp));
  bands.clear();
  lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x0000FF), 0);
  refresh();
  TEST_ASSERT_FALSE(bands.empty());
  TEST_ASSERT_EQUAL(0, bands.front().y0);
  TEST_ASSERT_EQUAL(kH, bands.back().y1);
  for (const Band& b : bands) TEST_ASSERT_LESS_OR_EQUAL(kStrip, b.y1 - b.y0);
  for (int y = 0; y < kH; ++y) {
    TEST_ASSERT_EQUAL_HEX16(rgb565(0x0000FF), panel[y][0]);
    TEST_ASSERT_EQUAL_HEX16(rgb565(0x0000FF), panel[y][curW - 1]);
  }
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_strip_has_no_frame);
  RUN_TEST(test_strip_pushes_full_width_rows_a_strip_at_a_time);
  RUN_TEST(test_strip_follows_a_resize);
  return UNITY_END();
}
