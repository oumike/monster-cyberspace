#include <unity.h>

#include <lvgl.h>

#include <vector>

#include "lvui/display.h"
#include "lvui/host.h"
#include "lvui/lv_init.h"

namespace {

uint32_t now = 0;
uint32_t nowMs() { return now; }

struct Band {
  const uint16_t* frame;
  int y0, y1;
};
std::vector<Band> bands;
void recordRows(const uint16_t* frame, int y0, int y1) { bands.push_back({frame, y0, y1}); }

lv_display_t* disp = nullptr;

void refresh() {
  now += 50;
  lv_refr_now(disp);
}

}  // namespace

void setUp() {
  lvui::setTickSource(nowMs);
  lvui::init();
  if (!disp) disp = lvui::displayBegin({64, 48, recordRows});
  lvui::displayResize(64, 48);
  lv_obj_clean(lv_screen_active());
  refresh();
  bands.clear();
}
void tearDown() {}

void test_flush_pushes_dirty_band() {
  TEST_ASSERT_NOT_NULL(disp);
  lv_obj_t* label = lv_label_create(lv_screen_active());
  lv_obj_set_pos(label, 0, 20);
  lv_obj_set_height(label, 17);
  lv_label_set_text(label, "a");
  refresh();
  bands.clear();

  lv_label_set_text(label, "b");
  refresh();
  TEST_ASSERT_FALSE(bands.empty());
  int lo = 1000, hi = -1;
  for (const Band& b : bands) {
    TEST_ASSERT_EQUAL_PTR(lvui::frame(), b.frame);
    TEST_ASSERT_NOT_EQUAL(0, b.y0);
    if (b.y0 < lo) lo = b.y0;
    if (b.y1 > hi) hi = b.y1;
  }
  TEST_ASSERT_LESS_OR_EQUAL(20, lo);
  TEST_ASSERT_GREATER_OR_EQUAL(37, hi);
  TEST_ASSERT_LESS_OR_EQUAL(48, hi);
}

void test_resize_rebuilds() {
  lvui::ScreenHost host;
  int builds = 0;
  TEST_ASSERT_TRUE(host.show([&](lv_obj_t* root) {
    ++builds;
    lv_label_set_text(lv_label_create(root), "x");
  }));
  TEST_ASSERT_EQUAL(1, builds);

  lvui::displayResize(32, 64);
  TEST_ASSERT_EQUAL(32, lv_display_get_horizontal_resolution(disp));
  TEST_ASSERT_EQUAL(64, lv_display_get_vertical_resolution(disp));
  host.rebuild();
  TEST_ASSERT_EQUAL(2, builds);
  lv_obj_update_layout(host.root());
  TEST_ASSERT_EQUAL(32, lv_obj_get_width(host.root()));

  bands.clear();
  refresh();
  TEST_ASSERT_FALSE(bands.empty());
  TEST_ASSERT_EQUAL(0, bands.front().y0);
  TEST_ASSERT_EQUAL(64, bands.back().y1);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_flush_pushes_dirty_band);
  RUN_TEST(test_resize_rebuilds);
  return UNITY_END();
}
