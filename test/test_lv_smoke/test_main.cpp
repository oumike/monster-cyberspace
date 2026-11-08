#include <unity.h>

#include <lvgl.h>

#include "lvui/lv_init.h"

static uint32_t fakeNow = 0;
static uint32_t fakeMs() { return fakeNow; }

static int flushes = 0;
static void dummyFlush(lv_display_t *disp, const lv_area_t *, uint8_t *) {
  ++flushes;
  lv_display_flush_ready(disp);
}

void setUp() {}
void tearDown() {}

void test_label_renders_and_flushes() {
  lvui::setTickSource(fakeMs);
  lvui::init();

  static uint8_t buf[64 * 32 * 2];
  lv_display_t *disp = lv_display_create(64, 32);
  TEST_ASSERT_NOT_NULL(disp);
  lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
  lv_display_set_buffers(disp, buf, nullptr, sizeof(buf), LV_DISPLAY_RENDER_MODE_FULL);
  lv_display_set_flush_cb(disp, dummyFlush);

  lv_obj_t *label = lv_label_create(lv_screen_active());
  TEST_ASSERT_NOT_NULL(label);
  lv_label_set_text(label, "hi");

  fakeNow += 100;
  lv_timer_handler();
  TEST_ASSERT_GREATER_OR_EQUAL(1, flushes);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_label_renders_and_flushes);
  return UNITY_END();
}
