#pragma once

#include <lvgl.h>

#include <string>

#include "lvui/views/list_box.h"
#include "lvui/views/view_util.h"
#include "ui/widgets/overlay.h"

namespace lvui {

// rc::Overlay as a modal box on a layer (lv_layer_top() by default): a
// clickable backdrop over the whole screen, so no tap reaches what's beneath,
// and the titled list placed as rc::Overlay::render places it. Rows emit
// {Activate, row, Menu, then Select}; the backdrop (and the title) Back.
// Hidden while the overlay is closed.
class MenuOverlay : public Tappable {
 public:
  MenuOverlay() = default;
  MenuOverlay(const MenuOverlay&) = delete;
  MenuOverlay& operator=(const MenuOverlay&) = delete;

  void create(lv_obj_t* layer = lv_layer_top());
  // Where the box goes. bottomPx: the screen's content bottom (above a hint
  // bar), -1 for the layer's bottom; rightReservePx: kept clear at the right.
  void setPlace(rc::OverlayPlace p, int bottomPx = -1, int rightReservePx = 0);
  void sync(const rc::Overlay& m);

  lv_obj_t* backdrop() const { return backdrop_; }
  lv_obj_t* box() const { return box_; }
  lv_obj_t* title() const { return title_; }
  ListBox& list() { return list_; }

  void tapped(lv_obj_t* obj, bool hold) override;

 private:
  void place(const rc::Overlay& m);

  lv_obj_t *backdrop_ = nullptr, *box_ = nullptr, *title_ = nullptr;
  ListBox list_;
  rc::OverlayPlace place_ = rc::OverlayPlace::BottomRight;
  int bottomPx_ = -1, rightPx_ = 0;
  // What the box was last placed from.
  bool placed_ = false;
  int lastN_ = -1, lastW_ = -1, lastH_ = -1;
  std::u32string lastTitle_;
};

}  // namespace lvui
