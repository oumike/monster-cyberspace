#pragma once

#include <lvgl.h>

#include <cstddef>
#include <functional>

namespace lvui {

// Holds the screen's object tree: each show() builds a new tree beside the
// old one and swaps only if it fit in memory.
class ScreenHost {
 public:
  static constexpr size_t kMinFree = 8192;
  static constexpr uint32_t kOomMs = 3000;

  // Builds into a new full-screen root. Out of memory: deletes the new tree,
  // keeps the old one, shows "Out of memory" for kOomMs and returns false.
  bool show(const std::function<void(lv_obj_t*)>& build);
  lv_obj_t* root() { return root_; }
  // Runs the last build that succeeded again (after a resize or theme change).
  void rebuild();

  // Test seam: what free LVGL memory is. Default: lv_mem_monitor's free_size,
  // or "plenty" with the C library allocator, which can't report it.
  static void setFreeProbe(size_t (*probe)());

 private:
  lv_obj_t* root_ = nullptr;
  std::function<void(lv_obj_t*)> build_;
};

}  // namespace lvui
