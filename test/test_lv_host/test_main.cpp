#include <unity.h>

#include <lvgl.h>

#include <cstring>

#include "lvui/display.h"
#include "lvui/host.h"
#include "lvui/input.h"
#include "lvui/lv_init.h"

namespace {

uint32_t now = 0;
uint32_t nowMs() { return now; }
void noRows(const uint16_t*, int, int) {}

bool starved = false;
size_t freeProbe() { return starved ? 1024 : 1 << 20; }

lv_obj_t* findOom() {
  lv_obj_t* top = lv_layer_top();
  for (uint32_t i = 0; i < lv_obj_get_child_count(top); ++i) {
    lv_obj_t* c = lv_obj_get_child(top, i);
    if (lv_obj_check_type(c, &lv_label_class) && strcmp(lv_label_get_text(c), "Out of memory") == 0)
      return c;
  }
  return nullptr;
}

rc::KeyEvent got;
int sunk = 0;
void sink(const rc::KeyEvent& e) {
  got = e;
  ++sunk;
}

}  // namespace

void setUp() {
  lvui::setTickSource(nowMs);
  lvui::init();
  static lv_display_t* disp = lvui::displayBegin({48, 32, noRows});
  (void)disp;
  lvui::ScreenHost::setFreeProbe(freeProbe);
  starved = false;
}
void tearDown() {}

void test_host_build_failure_keeps_previous() {
  lvui::ScreenHost host;
  TEST_ASSERT_TRUE(host.show([](lv_obj_t* root) { lv_label_create(root); }));
  lv_obj_t* old = host.root();
  TEST_ASSERT_EQUAL_PTR(lv_screen_active(), lv_obj_get_parent(old));
  const uint32_t children = lv_obj_get_child_count(lv_screen_active());

  TEST_ASSERT_FALSE(host.show([](lv_obj_t* root) {
    lv_label_create(root);
    starved = true;
  }));
  TEST_ASSERT_EQUAL_PTR(old, host.root());
  TEST_ASSERT_EQUAL_PTR(lv_screen_active(), lv_obj_get_parent(old));
  TEST_ASSERT_EQUAL(children, lv_obj_get_child_count(lv_screen_active()));
  TEST_ASSERT_NOT_NULL(findOom());

  starved = false;
  now += lvui::ScreenHost::kOomMs + 10;
  lv_timer_handler();
  TEST_ASSERT_NULL(findOom());

  // A later rebuild re-runs the last build that fit, not the failed one.
  int builds = 0;
  TEST_ASSERT_TRUE(host.show([&](lv_obj_t*) { ++builds; }));
  host.rebuild();
  TEST_ASSERT_EQUAL(2, builds);
  TEST_ASSERT_EQUAL(children, lv_obj_get_child_count(lv_screen_active()));
}

void test_emit_reaches_sink() {
  lvui::emit(rc::KeyEvent{});
  lvui::setKeySink(sink);
  rc::KeyEvent e;
  e.action = rc::Action::Activate;
  e.index = 3;
  lvui::emit(e);
  TEST_ASSERT_EQUAL(1, sunk);
  TEST_ASSERT_TRUE(got.action == rc::Action::Activate);
  TEST_ASSERT_EQUAL(3, got.index);
  lvui::setKeySink(nullptr);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_host_build_failure_keeps_previous);
  RUN_TEST(test_emit_reaches_sink);
  return UNITY_END();
}
