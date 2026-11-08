#pragma once

#include <lvgl.h>

#include <vector>

#include "lvui/views/row_gestures.h"
#include "lvui/views/view_util.h"
#include "ui/widgets/entry_list.h"

namespace lvui {

// rc::EntryList as the grid draws it: the whole two-line entries its window
// holds (EntryList::window; as many as fit, the first line in the head font
// when one is set), each in its own colours (FeedRow fg1/fg2). The selected
// entry is checked and outlined (Role::Focused: today's cursor, which keeps
// the row's colours) while rc::cursorShown(). The last column holds the
// scroll bar. Nothing scrolls natively: touch is RowGestures on a wrapping
// ListId::Main, so a tap emits {Activate, entry, Main, then Select} (a long
// press, then Palette) and a drag or swipe steps the selection.
class EntryListBox : public RowGestures::Rows {
 public:
  EntryListBox() = default;
  EntryListBox(const EntryListBox&) = delete;
  EntryListBox& operator=(const EntryListBox&) = delete;

  // Place obj() over the list's cells, its bar column included.
  void create(lv_obj_t* parent);
  // The font for each entry's first line (rc::EntryList::setBigHeads: the
  // next size up); null keeps the inherited one.
  void setHeadFont(const lv_font_t* f);
  // m must outlive the box: touch reads its selection.
  void sync(const rc::EntryList& m);

  lv_obj_t* obj() const { return box_; }
  // The k-th entry shown, from the top.
  lv_obj_t* row(int k) const { return rows_[k].row; }
  lv_obj_t* line1(int k) const { return rows_[k].line1; }
  lv_obj_t* line2(int k) const { return rows_[k].line2; }
  const ScrollBarView& scrollBar() const { return bar_; }

  int itemAt(lv_point_t p) const override;
  int shownItems() const override { return shown_; }
  int selectedItem() const override;
  int itemCount() const override;

 private:
  struct Row {
    lv_obj_t *row, *line1, *line2;
    int entry;
  };
  void styleHead(lv_obj_t* line1);
  lv_obj_t *box_ = nullptr, *lines_ = nullptr;
  ScrollBarView bar_;
  RowGestures touch_;
  std::vector<Row> rows_;
  int shown_ = 0, pitch_ = 0, width_ = 0;
  Cell cell_{0, 0};
  const lv_font_t* head_ = nullptr;
  const rc::EntryList* model_ = nullptr;
};

}  // namespace lvui
