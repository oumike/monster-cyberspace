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

// A desktop window (the CardputerZero's) can grow the panel to a whole
// monitor and back to the LCD's size; the frame regrows and every row draws.
void test_resize_to_monitor_and_back() {
  lvui::ScreenHost host;
  TEST_ASSERT_TRUE(host.show([](lv_obj_t* root) {
    for (int i = 0; i < 40; ++i) {
      lv_obj_t* l = lv_label_create(root);
      lv_obj_set_pos(l, (i % 8) * 200, (i / 8) * 40);
      lv_label_set_text(l, "monster");
    }
  }));
  const int sizes[][2] = {{1920, 1080}, {316, 168}, {2560, 1440}, {316, 168}};
  for (const auto& s : sizes) {
    lvui::displayResize(s[0], s[1]);
    TEST_ASSERT_EQUAL(s[0], lv_display_get_horizontal_resolution(disp));
    TEST_ASSERT_EQUAL(s[1], lv_display_get_vertical_resolution(disp));
    host.rebuild();
    TEST_ASSERT_NOT_NULL(host.root());
    lv_obj_update_layout(host.root());
    TEST_ASSERT_EQUAL(s[0], lv_obj_get_width(host.root()));
    TEST_ASSERT_EQUAL(s[1], lv_obj_get_height(host.root()));
    bands.clear();
    refresh();
    TEST_ASSERT_FALSE(bands.empty());
    int lo = s[1], hi = 0;
    for (const Band& b : bands) {
      if (b.y0 < lo) lo = b.y0;
      if (b.y1 > hi) hi = b.y1;
    }
    TEST_ASSERT_EQUAL(0, lo);
    TEST_ASSERT_EQUAL(s[1], hi);
  }
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_flush_pushes_dirty_band);
  RUN_TEST(test_resize_rebuilds);
  RUN_TEST(test_resize_to_monitor_and_back);
  return UNITY_END();
}
