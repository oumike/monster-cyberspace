#pragma once

#include <lvgl.h>

#include <string>
#include <vector>

#include "lvui/views/row_gestures.h"
#include "lvui/views/view_util.h"
#include "ui/widgets/drawer.h"

namespace lvui {

// rc::Drawer as a panel at the right of its parent, rc::Drawer::width columns
// wide and as tall as the parent: a drawer-edge rule at its left, the dim
// "here · N" header, then the window of names Drawer::render shows (the
// selection on the last row once it passes the bottom) in their nick colours
// (" z" when idle), and the scroll bar in the last column when they don't
// all fit. The selected name is checked and outlined while rc::cursorShown().
// Nothing scrolls natively: touch is RowGestures on a wrapping
// ListId::Drawer, so a tap on a row, edge column included, emits {Activate,
// row, Drawer, then Select} (a long press, then Palette) and a drag or swipe
// steps the selection. Docked, the header's ✕ emits {Activate, kCloseIndex,
// Drawer}.
class DrawerPanel : public Tappable, public RowGestures::Rows {
 public:
  DrawerPanel() = default;
  DrawerPanel(const DrawerPanel&) = delete;
  DrawerPanel& operator=(const DrawerPanel&) = delete;

  void create(lv_obj_t* parent);
  void setDocked(bool docked);
  // m must outlive the panel: touch reads its selection.
  void sync(const rc::Drawer& m);

  lv_obj_t* obj() const { return panel_; }
  lv_obj_t* header() const { return header_; }
  lv_obj_t* close() const { return close_; }
  // The k-th name shown, from the top.
  lv_obj_t* name(int k) const { return rows_[k].name; }
  lv_obj_t* row(int k) const { return rows_[k].row; }
  const ScrollBarView& scrollBar() const { return bar_; }

  void tapped(lv_obj_t* obj, bool hold) override;

  int itemAt(lv_point_t p) const override;
  int shownItems() const override { return shown_; }
  int selectedItem() const override;
  int itemCount() const override;

 private:
  struct Row {
    lv_obj_t *row, *name, *idle;
  };
  void makeBody();
  void layout();

  lv_obj_t *panel_ = nullptr, *head_ = nullptr, *header_ = nullptr, *close_ = nullptr;
  lv_obj_t* list_ = nullptr;
  ScrollBarView bar_;
  RowGestures touch_;
  std::vector<Row> rows_;
  bool docked_ = false;
  int inner_ = 0;     // columns right of the edge
  int listRows_ = 0;  // rows under the header
  int rowCols_ = 0;   // the name columns, less the bar's when shown
  int first_ = 0, shown_ = 0;
  Cell cell_{0, 0};
  int lastW_ = -1, lastH_ = -1;
  const rc::Drawer* model_ = nullptr;
};

}  // namespace lvui
