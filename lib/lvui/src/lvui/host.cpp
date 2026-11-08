#include "lvui/host.h"

#include "lvui/styles.h"

namespace lvui {

namespace {

size_t lvglFree() {
#if LV_USE_STDLIB_MALLOC == LV_STDLIB_CLIB
  return SIZE_MAX;
#else
  lv_mem_monitor_t mon;
  lv_mem_monitor(&mon);
  return mon.free_size;
#endif
}

size_t (*freeProbe)() = lvglFree;
lv_obj_t* oomLabel = nullptr;

void hideOom(lv_timer_t*) {
  lv_obj_delete(oomLabel);
  oomLabel = nullptr;
}

void showOom() {
  static lv_timer_t* timer = nullptr;
  if (!oomLabel) {
    oomLabel = lv_label_create(lv_layer_top());
    if (!oomLabel) return;
    addStyle(oomLabel, Role::Error);
    lv_label_set_text(oomLabel, "Out of memory");
    lv_obj_center(oomLabel);
    timer = lv_timer_create(hideOom, ScreenHost::kOomMs, nullptr);
    lv_timer_set_repeat_count(timer, 1);
  } else {
    lv_timer_reset(timer);
  }
}

}  // namespace

void ScreenHost::setFreeProbe(size_t (*probe)()) { freeProbe = probe ? probe : lvglFree; }

bool ScreenHost::show(const std::function<void(lv_obj_t*)>& build) {
  lv_obj_t* fresh = lv_obj_create(lv_screen_active());
  if (fresh) {
    lv_obj_remove_style_all(fresh);
    addStyle(fresh, Role::Screen);
    lv_obj_set_size(fresh, lv_pct(100), lv_pct(100));
    lv_obj_set_scrollbar_mode(fresh, LV_SCROLLBAR_MODE_OFF);
    build(fresh);
    if (freeProbe() >= kMinFree) {
      if (root_) lv_obj_delete(root_);
      root_ = fresh;
      if (&build != &build_) build_ = build;
      return true;
    }
    lv_obj_delete(fresh);
  }
  showOom();
  return false;
}

void ScreenHost::rebuild() {
  if (build_) show(build_);
}

}  // namespace lvui
