#pragma once

#include <lvgl.h>

#include <cstddef>
#include <functional>

namespace lvui {

// Holds the screen's object tree: each show() builds a new tree beside the
// old one and swaps only if it fit in memory. With setReplaceFirst (boards
// without PSRAM) the old tree goes first, so two never stand at once.
class ScreenHost {
 public:
  // Left free once a tree is built, or it is thrown away.
  static constexpr size_t kMinFree = 8192;
  // Needed free before a build starts. Past LVGL's budget an object can't be
  // made, and views don't check for that, so a build must never get there:
  // the largest tree (a Room on the Cardputer, about 17 KB) plus kMinFree.
  static constexpr size_t kBuildFree = 28672;
  static constexpr uint32_t kOomMs = 3000;
  // With no tree, how often retryDue() lets the caller try again.
  static constexpr uint32_t kRetryMs = 1000;

  // Builds into a new full-screen root. Out of memory: deletes the new tree,
  // keeps the old one, shows "Out of memory" for kOomMs and returns false.
  // Replace first: out of memory leaves no tree (root() null) and the
  // caller builds the old screen again.
  bool show(const std::function<void(lv_obj_t*)>& build);
  lv_obj_t* root() { return root_; }
  // Runs the last build that succeeded again (after a resize or theme change):
  // beside the old tree, which stays if it doesn't fit, or (replace first)
  // after it, like show().
  void rebuild();
  // With no tree (replace first ran out of memory twice): true once
  // kRetryMs has passed since the last build failed, so the caller builds
  // again at most once a second. False while there is a tree.
  bool retryDue();

  // Test seam: what free LVGL memory is. Default: lv_mem_monitor's free_size
  // (without PSRAM, no more than the internal heap has free), or "plenty"
  // with the C library allocator, which can't report it.
  static void setFreeProbe(size_t (*probe)());
  static void setReplaceFirst(bool on);

 private:
  bool show(const std::function<void(lv_obj_t*)>& build, bool replace);
  lv_obj_t* root_ = nullptr;
  std::function<void(lv_obj_t*)> build_;
  uint32_t failedAt_ = 0;
};

}  // namespace lvui
