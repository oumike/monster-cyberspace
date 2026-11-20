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

// Home's layout for a panel: edge-anchored buttons (rc::homeButtons, labels
// up to labelCells long) in big cells, then normal ones; failing those one
// scrolling column of tiles (rc::homeColumn, gapped while it shows three).
// Both btns.rects and lay.tiles are empty when nothing fits. spanFirst:
// as rc::homeButtons'.
struct HomeFit {
  rc::HomeButtons btns;
  rc::HomeLayout lay;  // the column, when column
  bool big = false, column = false;
  rc::TextSize ts = rc::TextSize::Comfortable;
  rc::CellSize cell{0, 0};  // the buttons' or tiles' cell (big or normal)
};
HomeFit homeFit(int panelW, int areaPx, int gridRows, rc::TextSize ts, int count, int percent,
                int labelCells, bool spanFirst = false);

// A Home button's label; unread: something waits there, and the button is
// highlighted (filled in the selection colour, or a 2 px outline on
// outlined themes).
struct HomeTileInfo {
  std::u32string label;
  bool unread = false;
  bool operator==(const HomeTileInfo& o) const { return unread == o.unread && label == o.label; }
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
    bool filled = false;  // bg fill instead of the theme's bg
    uint16_t fill = 0;
    uint8_t edgeW = 1;  // the outline's width in px
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

  // Home's buttons from fit (homeFit), from item first on (the column): row1Px
  // is where the area under the status bar starts. selected -1: no cursor.
  // arrows: ▲/▼ on a column's outline where more tiles lie beyond (e-paper).
  // bg: as syncTiles' (a page that scrolls takes drags off the buttons too).
  void syncHome(const HomeFit& fit, int row1Px, const std::vector<HomeTileInfo>& infos, int first,
                int selected, bool arrows = false, lv_area_t bg = {0, 0, -1, -1});

  // ButtonPad's buttons in b (rc::buttonGrid over grid rows [top, bottom),
  // row top at topPx): the label (error, marker accent or fg), `right` dim
  // at the right (keepRight: split the room), the detail dim under it.
  // valueBelow: `right` dim on the row under the button instead (b laid out
  // with below 1), where a tap picks the item too.
  // selected -1: no cursor. The scroll bar down the last column from
  // rc::drawScrollBar's total/visible/first.
  struct Scroll {
    int total = 0, visible = 0, first = 0;
  };
  void syncButtons(const rc::ButtonGrid& b, const std::vector<rc::ListItem>& items, int selected,
                   bool keepRight, int top, int bottom, int topPx, Scroll scroll, bool valueBelow = false);

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
  lv_area_t homeBg_{0, 0, -1, -1};
  bool arrows_ = false;
  std::vector<HomeTileInfo> infos_;
  rc::ButtonGrid grid_;
  std::vector<rc::ListItem> items_;
  bool keepRight_ = false;
  bool valueBelow_ = false;
  int top_ = 0, bottom_ = 0, topPx_ = 0;
  Scroll scroll_;
};

}  // namespace lvui
