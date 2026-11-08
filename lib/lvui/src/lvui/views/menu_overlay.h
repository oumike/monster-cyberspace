#pragma once

#include <lvgl.h>

#include <string>

#include "lvui/views/list_box.h"
#include "lvui/views/tile_grid.h"
#include "lvui/views/view_util.h"
#include "ui/widgets/overlay.h"

namespace lvui {

// rc::Overlay as a modal box on a layer (lv_layer_top() by default): a
// clickable backdrop over the whole screen, so no tap reaches what's beneath,
// and the titled list placed as rc::Overlay::render places it. Rows emit
// {Activate, row, Menu, then Select}; the backdrop (and the title) Back.
// Hidden while the overlay is closed. On touch-only boards
// (rc::Overlay::confirmButtons) a confirm or a button menu draws as the
// grid's buttons instead (a TileGrid on the Menu list), with the title on
// the page: a confirm's wrapped in the error colour over a panel.
class MenuOverlay : public Tappable {
 public:
  MenuOverlay() = default;
  MenuOverlay(const MenuOverlay&) = delete;
  MenuOverlay& operator=(const MenuOverlay&) = delete;

  void create(lv_obj_t* layer = lv_layer_top());
  // Where the box goes. bottomPx: the screen's content bottom (above a hint
  // bar), -1 for the layer's bottom; rightReservePx: kept clear at the right;
  // topPx: the content top (below a status bar), which buttons stay under.
  void setPlace(rc::OverlayPlace p, int bottomPx = -1, int rightReservePx = 0, int topPx = 0);
  void sync(const rc::Overlay& m);

  lv_obj_t* backdrop() const { return backdrop_; }
  lv_obj_t* box() const { return box_; }
  lv_obj_t* title() const { return title_; }
  ListBox& list() { return list_; }
  TileGrid& buttons() { return grid_; }
  lv_obj_t* buttonTitle() const { return buttonTitle_; }

  void tapped(lv_obj_t* obj, bool hold) override;

 private:
  void place(const rc::Overlay& m);
  bool syncButtons(const rc::Overlay& m);
  bool layoutButtons(const rc::Overlay& m);

  lv_obj_t *backdrop_ = nullptr, *box_ = nullptr, *title_ = nullptr;
  ListBox list_;
  rc::OverlayPlace place_ = rc::OverlayPlace::BottomRight;
  int bottomPx_ = -1, rightPx_ = 0, topPx_ = 0;
  // What the box was last placed from.
  bool placed_ = false;
  int lastN_ = -1, lastW_ = -1, lastH_ = -1;
  std::u32string lastTitle_;

  // The touch-only buttons, and what they were last laid out from.
  lv_obj_t *panel_ = nullptr, *buttonTitle_ = nullptr;
  TileGrid grid_;
  bool btnLaid_ = false, btnOk_ = false, btnConfirm_ = false;
  int btnW_ = -1, btnH_ = -1;
  Cell btnCell_{};
  unsigned btnTheme_ = ~0u;
  std::u32string btnTitle_;
  std::vector<std::pair<std::u32string, rc::ItemTone>> btnItems_;
  std::vector<TileGrid::Tile> btnTiles_;
};

}  // namespace lvui
