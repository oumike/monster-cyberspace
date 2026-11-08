#pragma once

#include <lvgl.h>

#include <vector>

#include "lvui/views/row_gestures.h"
#include "lvui/views/view_util.h"
#include "ui/widgets/list_view.h"

namespace lvui {

// rc::ListView as a column of one-cell-high rows: "• label  right ". The
// selected row is checked (Role::Focused: outlined in selectBg, keeping its
// own colours, as the grid's list_view does) while rc::cursorShown(). Two
// modes, picked at create():
// - scrolling (a menu): every row, the box scrolled to keep the selection in
//   view. A tap on a row emits {Activate, row, list, then Select}; a long
//   press, then Palette (input/touch_resolve).
// - windowed (a screen's list): ListView::render's window, the rows that fit
//   with the selection kept on the last, and a scroll bar in the last column
//   when they overflow. Nothing scrolls natively: touch is RowGestures on a
//   wrapping list (TapMap::setList wraps), so a tap or long press emits as
//   above and a drag or swipe steps the selection.
class ListBox : public Tappable, public RowGestures::Rows {
 public:
  ListBox() = default;
  ListBox(const ListBox&) = delete;
  ListBox& operator=(const ListBox&) = delete;

  // Scrolling: a full-width column as tall as its rows; size it to scroll.
  // Windowed: place obj() over the list's cells, its bar column included.
  void create(lv_obj_t* parent, bool windowed = false);
  // The list its rows' taps name (rc::ListView::setListId). Default Main;
  // windowed, set it before create().
  void setListId(rc::ListId id) { list_ = id; }
  // Windowed: m must outlive the box, as touch reads its selection.
  void sync(const rc::ListView& m);

  lv_obj_t* obj() const { return box_; }
  // The i-th row shown, from the top (scrolling: item i).
  lv_obj_t* label(int i) const { return rows_[i].label; }
  lv_obj_t* right(int i) const { return rows_[i].right; }
  const ScrollBarView& scrollBar() const { return bar_; }

  void tapped(lv_obj_t* obj, bool hold) override;
  int itemAt(lv_point_t p) const override;
  int shownItems() const override { return shown_; }
  int selectedItem() const override;
  int itemCount() const override;

 private:
  struct Row {
    lv_obj_t *row, *marker, *label, *right;
    bool error;
  };
  Row makeRow(lv_obj_t* parent);
  void fillRow(Row& r, const rc::ListItem& it);
  void syncWindow(const rc::ListView& m);

  lv_obj_t *box_ = nullptr, *lines_ = nullptr;
  std::vector<Row> rows_;
  rc::ListId list_ = rc::ListId::Main;
  int sel_ = -1;
  // Windowed.
  bool windowed_ = false;
  ScrollBarView bar_;
  RowGestures touch_;
  int shown_ = 0, first_ = 0, width_ = 0;
  Cell cell_{0, 0};
  const rc::ListView* model_ = nullptr;
};

}  // namespace lvui
