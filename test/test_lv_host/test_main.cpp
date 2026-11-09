#include <unity.h>

#include <lvgl.h>

#include <cstring>

#include "lvui/display.h"
#include "lvui/host.h"
#include "lvui/input.h"
#include "lvui/lv_init.h"
#include "lvui/tree_views.h"

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

// Boards without PSRAM: the old tree goes before the new one is built, so
// two never stand at once. A build that doesn't fit leaves no tree; the
// caller builds the old screen again.
void test_host_replace_first() {
  lvui::ScreenHost::setReplaceFirst(true);
  lvui::ScreenHost host;
  const uint32_t before = lv_obj_get_child_count(lv_screen_active());
  TEST_ASSERT_TRUE(host.show([](lv_obj_t* root) { lv_label_create(root); }));
  lv_obj_t* old = host.root();
  bool oldGone = false;
  lv_obj_add_event_cb(
      old, [](lv_event_t* e) { *static_cast<bool*>(lv_event_get_user_data(e)) = true; }, LV_EVENT_DELETE,
      &oldGone);
  TEST_ASSERT_TRUE(host.show([&](lv_obj_t*) { TEST_ASSERT_TRUE(oldGone); }));
  TEST_ASSERT_EQUAL(before + 1, lv_obj_get_child_count(lv_screen_active()));

  // A rebuild of the same screen still builds beside: out of memory keeps it.
  lv_obj_t* kept = host.root();
  starved = true;
  host.rebuild();
  TEST_ASSERT_EQUAL_PTR(kept, host.root());
  starved = false;

  TEST_ASSERT_FALSE(host.show([](lv_obj_t*) { starved = true; }));
  TEST_ASSERT_NULL(host.root());
  TEST_ASSERT_EQUAL(before, lv_obj_get_child_count(lv_screen_active()));
  TEST_ASSERT_NOT_NULL(findOom());
  starved = false;
  lvui::ScreenHost::setReplaceFirst(false);
}

// A screen's views live while their tree does: the newest tree's are current,
// and when a new tree is thrown away the previous one's come back.
void test_tree_views_follow_their_tree() {
  struct Views {
    int n = 0;
  };
  lvui::TreeViews<Views> v;
  TEST_ASSERT_NULL(v.get());
  lv_obj_t* a = lv_obj_create(lv_screen_active());
  v.make(a).n = 1;
  lv_obj_t* b = lv_obj_create(lv_screen_active());
  v.make(b).n = 2;
  TEST_ASSERT_EQUAL(2, v->n);
  lv_obj_delete(b);  // the new tree didn't fit
  TEST_ASSERT_EQUAL(1, v->n);
  lv_obj_t* c = lv_obj_create(lv_screen_active());
  v.make(c).n = 3;
  lv_obj_delete(a);  // the old tree goes once the new one fits
  TEST_ASSERT_EQUAL(3, v->n);
  lv_obj_delete(c);
  TEST_ASSERT_NULL(v.get());
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

void test_oom_label_survives_layer_clean() {
  lvui::ScreenHost host;
  TEST_ASSERT_TRUE(host.show([](lv_obj_t*) {}));
  TEST_ASSERT_FALSE(host.show([](lv_obj_t*) { starved = true; }));
  TEST_ASSERT_NOT_NULL(findOom());
  // Whoever owns the top layer clears it: the label goes, and its timer with it.
  lv_obj_clean(lv_layer_top());
  now += lvui::ScreenHost::kOomMs + 10;
  lv_timer_handler();
  starved = false;
  TEST_ASSERT_FALSE(host.show([](lv_obj_t*) { starved = true; }));
  TEST_ASSERT_NOT_NULL(findOom());
  now += lvui::ScreenHost::kOomMs + 10;
  lv_timer_handler();
  TEST_ASSERT_NULL(findOom());
  starved = false;
}

int pointers() {
  int n = 0;
  for (lv_indev_t* i = lv_indev_get_next(nullptr); i; i = lv_indev_get_next(i))
    if (lv_indev_get_type(i) == LV_INDEV_TYPE_POINTER) ++n;
  return n;
}
bool noTouch(int16_t&, int16_t&) { return false; }

void test_pointer_made_once() {
  const int before = pointers();
  lvui::pointerBegin(nullptr);
  TEST_ASSERT_FALSE(lvui::hasPointer());
  lvui::pointerBegin(noTouch);
  lvui::pointerBegin(noTouch);
  TEST_ASSERT_TRUE(lvui::hasPointer());
  TEST_ASSERT_EQUAL(before + 1, pointers());
}

const char* assertFile = nullptr;
int assertLine = 0;
void onAssert(const char* file, int line) {
  assertFile = file;
  assertLine = line;
}

// LVGL's asserts reach the handler with where they failed (no silent hang).
void test_assert_reaches_handler() {
  lvui::setAssertHandler(onAssert);
  void* none = nullptr;
  const int line = __LINE__ + 1;
  LV_ASSERT_NULL(none);
  TEST_ASSERT_NOT_NULL(assertFile);
  TEST_ASSERT_NOT_NULL(strstr(assertFile, "test_lv_host"));
  TEST_ASSERT_EQUAL(line, assertLine);
  lvui::setAssertHandler(nullptr);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_host_build_failure_keeps_previous);
  RUN_TEST(test_host_replace_first);
  RUN_TEST(test_tree_views_follow_their_tree);
  RUN_TEST(test_emit_reaches_sink);
  RUN_TEST(test_oom_label_survives_layer_clean);
  RUN_TEST(test_pointer_made_once);
  RUN_TEST(test_assert_reaches_handler);
  return UNITY_END();
}
