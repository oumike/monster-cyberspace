// SPDX-License-Identifier: GPL-3.0-or-later
#include <unity.h>

#include "ui/window_fit.h"

using namespace rc::winfit;

void setUp() {}
void tearDown() {}

void test_zoom_clamped() {
  TEST_ASSERT_EQUAL(1, clampZoom(0));
  TEST_ASSERT_EQUAL(1, clampZoom(1));
  TEST_ASSERT_EQUAL(2, clampZoom(2));
  TEST_ASSERT_EQUAL(3, clampZoom(3));
  TEST_ASSERT_EQUAL(3, clampZoom(9));
  TEST_ASSERT_EQUAL(1, clampZoom(-4));
}

void test_zoom_setting_keeps_auto() {
  TEST_ASSERT_EQUAL(kZoomAuto, zoomSetting(0));
  TEST_ASSERT_EQUAL(2, zoomSetting(2));
  TEST_ASSERT_EQUAL(3, zoomSetting(9));
}

void test_auto_zoom_follows_the_window() {
  TEST_ASSERT_EQUAL(1, autoZoom(640, 340));     // the LCD's 320x170 at 2x
  TEST_ASSERT_EQUAL(1, autoZoom(1599, 899));
  TEST_ASSERT_EQUAL(2, autoZoom(1600, 900));
  TEST_ASSERT_EQUAL(2, autoZoom(1920, 1080));
  TEST_ASSERT_EQUAL(3, autoZoom(2560, 1440));
  TEST_ASSERT_EQUAL(3, autoZoom(2940, 1600));   // a maximised window on a HiDPI laptop
  TEST_ASSERT_EQUAL(4, autoZoom(3840, 2160));
  TEST_ASSERT_EQUAL(6, autoZoom(7680, 4320));   // capped
  TEST_ASSERT_EQUAL(1, autoZoom(3000, 800));    // the short side decides
  const Fit f = fit(1920, 1080, kZoomAuto);
  TEST_ASSERT_EQUAL(2, f.zoom);
  TEST_ASSERT_EQUAL(960, f.panelW);
  TEST_ASSERT_EQUAL(540, f.panelH);
  int w, h;
  minWindow(316, 168, kZoomAuto, w, h);
  TEST_ASSERT_EQUAL(316, w);
  TEST_ASSERT_EQUAL(168, h);
}

void test_panel_rounds_down_and_centres() {
  const Fit f = fit(1001, 600, 2);
  TEST_ASSERT_EQUAL(500, f.panelW);
  TEST_ASSERT_EQUAL(300, f.panelH);
  TEST_ASSERT_EQUAL(0, f.offX);  // one spare pixel: no room to centre
  TEST_ASSERT_EQUAL(0, f.offY);
  const Fit g = fit(1920, 1080, 3);
  TEST_ASSERT_EQUAL(640, g.panelW);
  TEST_ASSERT_EQUAL(360, g.panelH);
  const Fit h = fit(1925, 1085, 3);  // 5 spare each way
  TEST_ASSERT_EQUAL(641, h.panelW);
  TEST_ASSERT_EQUAL(361, h.panelH);
  TEST_ASSERT_EQUAL(1, h.offX);
  TEST_ASSERT_EQUAL(1, h.offY);
  TEST_ASSERT_EQUAL(3, h.zoom);
}

void test_panel_never_empty() {
  const Fit f = fit(1, 0, 3);
  TEST_ASSERT_EQUAL(1, f.panelW);
  TEST_ASSERT_EQUAL(1, f.panelH);
  TEST_ASSERT_EQUAL(3, fit(100, 100, 7).zoom);
}

void test_point_inside() {
  const Fit f = fit(1925, 1085, 3);
  int16_t x, y;
  TEST_ASSERT_TRUE(toPanel(f, 1, 1, x, y));
  TEST_ASSERT_EQUAL(0, x);
  TEST_ASSERT_EQUAL(0, y);
  TEST_ASSERT_TRUE(toPanel(f, 1 + 3 * 10 + 2, 1 + 3 * 20, x, y));
  TEST_ASSERT_EQUAL(10, x);
  TEST_ASSERT_EQUAL(20, y);
}

void test_point_in_margin_clamped() {
  const Fit f = fit(1925, 1085, 3);
  int16_t x, y;
  TEST_ASSERT_FALSE(toPanel(f, 0, 0, x, y));
  TEST_ASSERT_EQUAL(0, x);
  TEST_ASSERT_EQUAL(0, y);
  TEST_ASSERT_FALSE(toPanel(f, 1924, 1084, x, y));
  TEST_ASSERT_EQUAL(640, x);
  TEST_ASSERT_EQUAL(360, y);
}

void test_min_window() {
  int w, h;
  minWindow(316, 168, 2, w, h);
  TEST_ASSERT_EQUAL(632, w);
  TEST_ASSERT_EQUAL(336, h);
  minWindow(316, 168, 9, w, h);
  TEST_ASSERT_EQUAL(948, w);
  TEST_ASSERT_EQUAL(504, h);
}

void test_wheel_steps() {
  TEST_ASSERT_EQUAL(3, wheelSteps(1));
  TEST_ASSERT_EQUAL(-6, wheelSteps(-2));
  TEST_ASSERT_EQUAL(0, wheelSteps(0));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_zoom_clamped);
  RUN_TEST(test_zoom_setting_keeps_auto);
  RUN_TEST(test_auto_zoom_follows_the_window);
  RUN_TEST(test_panel_rounds_down_and_centres);
  RUN_TEST(test_panel_never_empty);
  RUN_TEST(test_point_inside);
  RUN_TEST(test_point_in_margin_clamped);
  RUN_TEST(test_min_window);
  RUN_TEST(test_wheel_steps);
  return UNITY_END();
}
