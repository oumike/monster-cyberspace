#pragma once

#include <lvgl.h>

#include <cstdint>

#include "lvui/views/view_util.h"
#include "ui/widgets/side_nav.h"

namespace lvui {

// rc::renderSideNav as a strip at the right of its parent, rc::sideNavCols
// columns wide: three dim-outlined buttons, ▲ ● ▼, stacked as the grid splits
// the rows (the middle one takes what the even split leaves). Taps (and long
// presses) emit Up, Select and Down.
class SideNavStrip : public Tappable {
 public:
  SideNavStrip() = default;
  SideNavStrip(const SideNavStrip&) = delete;
  SideNavStrip& operator=(const SideNavStrip&) = delete;

  void create(lv_obj_t* parent);
  // Follows the parent's size, the font and the theme's dim colour.
  void sync();

  lv_obj_t* obj() const { return strip_; }
  lv_obj_t* button(int i) const { return buttons_[i]; }

  void tapped(lv_obj_t* obj, bool hold) override;

 private:
  lv_obj_t* strip_ = nullptr;
  lv_obj_t* buttons_[3] = {};
  int lastW_ = -1, lastH_ = -1, lastCellW_ = -1;
  int32_t dim_ = -1;
};

}  // namespace lvui
