#include "lvui/host.h"

#include "lvui/styles.h"
#include "lvui/views/view_util.h"

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
bool replaceFirst = false;
// The label and the timer that hides it live and die together: whichever
// goes first (the timer firing, or the label deleted with its layer) clears
// both pointers, so neither dangles.
lv_obj_t* oomLabel = nullptr;
lv_timer_t* oomTimer = nullptr;

void oomDeleted(lv_event_t*) {
  oomLabel = nullptr;
  if (oomTimer) lv_timer_delete(oomTimer);
  oomTimer = nullptr;
}

void hideOom(lv_timer_t*) {
  oomTimer = nullptr;  // a repeat count of 1: LVGL deletes it
  if (oomLabel) lv_obj_delete(oomLabel);
}

void showOom() {
  if (oomLabel && oomTimer) {
    lv_timer_reset(oomTimer);
    return;
  }
  if (!oomLabel) {
    oomLabel = lv_label_create(lv_layer_top());
    if (!oomLabel) return;
    lv_obj_add_event_cb(oomLabel, oomDeleted, LV_EVENT_DELETE, nullptr);
    addStyle(oomLabel, Role::Error);
    lv_label_set_text(oomLabel, "Out of memory");
    lv_obj_center(oomLabel);
  }
  oomTimer = lv_timer_create(hideOom, ScreenHost::kOomMs, nullptr);
  // No timer to hide it: don't leave the label up for good.
  if (!oomTimer) {
    lv_obj_delete(oomLabel);
    return;
  }
  lv_timer_set_repeat_count(oomTimer, 1);
}

}  // namespace

void ScreenHost::setFreeProbe(size_t (*probe)()) { freeProbe = probe ? probe : lvglFree; }

void ScreenHost::setReplaceFirst(bool on) { replaceFirst = on; }

bool ScreenHost::show(const std::function<void(lv_obj_t*)>& build) { return show(build, replaceFirst); }

bool ScreenHost::show(const std::function<void(lv_obj_t*)>& build, bool replace) {
  if (replace && root_) {
    lv_obj_delete(root_);
    root_ = nullptr;
  }
  lv_obj_t* fresh = lv_obj_create(lv_screen_active());
  if (fresh) {
    lv_obj_remove_style_all(fresh);
    addStyle(fresh, Role::Screen);
    lv_obj_set_size(fresh, lv_pct(100), lv_pct(100));
    lv_obj_set_scrollbar_mode(fresh, LV_SCROLLBAR_MODE_OFF);
    // The rows start below a touch header; its status bar reaches up over
    // the padding, which must not make the root scroll.
    lv_obj_remove_flag(fresh, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_top(fresh, header().extra, LV_PART_MAIN);
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
  if (build_) show(build_, false);
}

}  // namespace lvui
