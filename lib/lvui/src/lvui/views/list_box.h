#pragma once

#include <lvgl.h>

#include <vector>

#include "lvui/views/row_gestures.h"
#include "lvui/views/view_util.h"
#include "ui/widgets/list_view.h"

namespace lvui {

// rc::ListView as ListView::render draws it: over the box's cells, the rows
// that fit, the selection kept on the last once it passes it, each a
// one-cell-high "• label  right " row, and a scroll bar in the last column
// when the list is longer than the rows. The selected row is checked
// (Role::Focused: outlined in selectBg, keeping its own colours, as the
// grid's list_view does) while rc::cursorShown(). Nothing scrolls natively:
// touch is RowGestures on a wrapping list (ListView::render's TapMap::setList
// wraps), so a tap on a row emits {Activate, row, list, then Select}, a long
// press then Palette (input/touch_resolve), and a drag or swipe steps the
// selection.
class ListBox : public RowGestures::Rows {
 public:
  ListBox() = default;
  ListBox(const ListBox&) = delete;
  ListBox& operator=(const ListBox&) = delete;

  // Place obj() over the list's cells, its bar column included (a flex
  // child that grows works too).
  void create(lv_obj_t* parent);
  // The list its rows' keys name (rc::ListView::setListId). Default Main.
  void setListId(rc::ListId id);
  // A tap or long press on the box but on no row (the bar column, below
  // the last row) emits Back, as an overlay's backdrop under its list does.
  void setMissBack(bool on) { missBack_ = on; }
  // m must outlive the box, as touch reads its selection.
  void sync(const rc::ListView& m);

  lv_obj_t* obj() const { return box_; }
  // The i-th row shown, from the top.
  lv_obj_t* row(int i) const { return rows_[i].row; }
  lv_obj_t* label(int i) const { return rows_[i].label; }
  lv_obj_t* right(int i) const { return rows_[i].right; }
  // The item shown on the top row.
  int firstShown() const { return first_; }
  const ScrollBarView& scrollBar() const { return bar_; }

  int itemAt(lv_point_t p) const override;
  int shownItems() const override { return shown_; }
  int selectedItem() const override;
  int itemCount() const override;
  void missed(bool hold) override;

 private:
  struct Row {
    lv_obj_t *row, *marker, *label, *right;
    bool error;
  };
  Row makeRow(lv_obj_t* parent);
  void fillRow(Row& r, const rc::ListItem& it);

  lv_obj_t *box_ = nullptr, *lines_ = nullptr;
  std::vector<Row> rows_;
  rc::ListId list_ = rc::ListId::Main;
  bool missBack_ = false;
  ScrollBarView bar_;
  RowGestures touch_;
  int shown_ = 0, first_ = 0, width_ = 0;
  Cell cell_{0, 0};
  const rc::ListView* model_ = nullptr;
};

}  // namespace lvui
