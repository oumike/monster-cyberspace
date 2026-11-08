#pragma once

#include <lvgl.h>

#include <string>
#include <vector>

#include "input/key_event.h"
#include "lvui/views/row_gestures.h"
#include "lvui/views/view_util.h"
#include "ui/button_grid.h"
#include "ui/grid.h"
#include "ui/home_tiles.h"
#include "ui/widgets/list_view.h"

namespace lvui {

// Home's tile layout for a panel, as HomeScreen::render picks it: gapped
// tiles on big rows, then normal rows, then touching; failing those one
// scrolling column (gapped while it shows three tiles). lay.tiles is empty
// when nothing fits.
struct HomeFit {
  rc::HomeLayout lay;
  bool big = false, column = false;
  rc::TextSize ts = rc::TextSize::Comfortable;
  rc::CellSize cell{0, 0};  // the tile rows' cell (big or normal)
};
HomeFit homeFit(int panelW, int areaPx, int gridRows, rc::TextSize ts, int count, int percent,
                bool spanFirst);

// A Home tile's text: the label (the accent when unread) and the dim line
// under or beside it.
struct HomeTileInfo {
  std::u32string label, sub;
  bool unread = false;
  bool operator==(const HomeTileInfo& o) const {
    return unread == o.unread && label == o.label && sub == o.sub;
  }
};

// Outlined boxes of text in px, laid out by the grid models (Home's tiles,
// ButtonPad's buttons, a touch-only Overlay's buttons) and drawn as the grid
// draws them: bg inside, a 1 px outline in its edge colour, and on outlined
// themes a second box one cell in for the cursor. Fills its parent and takes
// every press there as rc::Gestures and the tap map do with Row targets on a
// non-hold-to-act list: a tap on a tile emits {Activate, index, list, then
// Select} (a long press: then Palette), drags and swipes step Up/Down. A tap
// on nothing does nothing, or emits Back with setMissBack.
class TileGrid : public RowGestures::Rows {
 public:
  struct Text {
    int x, y, w;  // px inside the tile; one cell high
    std::u32string text;
    uint16_t fg;
    bool opaque = false;  // drawn on bg, over the outline
    uint16_t bg = 0;
  };
  struct Tile {
    int x, y, w, h;  // px in the grid
    int index;       // the item a tap picks
    uint16_t edge;
    // The cursor's second box (px inside the tile), w 0 for none.
    int ix = 0, iy = 0, iw = 0, ih = 0;
    std::vector<Text> texts;
  };

  TileGrid() = default;
  TileGrid(const TileGrid&) = delete;
  TileGrid& operator=(const TileGrid&) = delete;

  void create(lv_obj_t* parent, rc::ListId list = rc::ListId::Main);
  void setMissBack(bool on) { missBack_ = on; }

  // The tiles as given. font: the tiles' text (nullptr: the grid's own);
  // shownRows: the rows the tap map would hold (a swipe's page of steps);
  // bgPx: an area (px in the grid, w 0 for none) that takes drags, a tap
  // there emitting index -1, as ButtonPad's background.
  void syncTiles(const std::vector<Tile>& tiles, const lv_font_t* font, int shownRows,
                 lv_area_t bg = {0, 0, -1, -1});

  // Home's tiles from fit (homeFit), from item first on: row1Px is where grid
  // row 1 (the pad above the tiles) starts. selected -1: no cursor. arrows:
  // ▲/▼ on a column's outline where more tiles lie beyond (e-paper).
  void syncHome(const HomeFit& fit, int row1Px, const std::vector<HomeTileInfo>& infos, int first,
                int selected, bool arrows = false);

  // ButtonPad's buttons in b (rc::buttonGrid over grid rows [top, bottom),
  // row top at topPx): the label (error, marker accent or fg), `right` dim
  // at the right (keepRight: split the room), the detail dim under it.
  // selected -1: no cursor. The scroll bar down the last column from
  // rc::drawScrollBar's total/visible/first.
  struct Scroll {
    int total = 0, visible = 0, first = 0;
  };
  void syncButtons(const rc::ButtonGrid& b, const std::vector<rc::ListItem>& items, int selected,
                   bool keepRight, int top, int bottom, int topPx, Scroll scroll);

  lv_obj_t* obj() const { return root_; }
  int count() const { return shown_; }
  lv_obj_t* tile(int i) const { return tiles_[i].box; }
  lv_obj_t* label(int i, int j) const { return tiles_[i].labels[j]; }

  // RowGestures::Rows
  int itemAt(lv_point_t p) const override;
  int shownItems() const override { return shownRows_; }
  bool onRows(lv_point_t p) const override;
  void missed(bool hold) override;

 private:
  struct TileObj {
    lv_obj_t* box = nullptr;
    lv_obj_t* inner = nullptr;
    std::vector<lv_obj_t*> labels;
    lv_area_t area{};  // px in the grid
    int index = -1;
  };

  lv_obj_t *root_ = nullptr, *sbHost_ = nullptr;
  RowGestures gestures_;
  ScrollBarView bar_;
  std::vector<TileObj> tiles_;
  int shown_ = 0, shownRows_ = 0;
  lv_area_t bg_{0, 0, -1, -1};
  bool missBack_ = false;

  // What syncHome / syncButtons last drew from.
  enum class Mode : uint8_t { None, Home, Buttons } mode_ = Mode::None;
  unsigned theme_ = ~0u;
  bool cursor_ = false;
  HomeFit fit_;
  int row1_ = 0, first_ = 0, sel_ = -1;
  bool arrows_ = false;
  std::vector<HomeTileInfo> infos_;
  rc::ButtonGrid grid_;
  std::vector<rc::ListItem> items_;
  bool keepRight_ = false;
  int top_ = 0, bottom_ = 0, topPx_ = 0;
  Scroll scroll_;
};

}  // namespace lvui
