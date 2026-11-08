#pragma once

#include <lvgl.h>

#include <vector>

#include "lvui/views/view_util.h"
#include "ui/widgets/entry_list.h"

namespace lvui {

// rc::EntryList as a scrolling column of two-line entries in each row's own
// colours (FeedRow fg1/fg2). The selected entry is checked and outlined
// (Role::Focused: today's cursor, which keeps the row's colours) while
// rc::cursorShown(), and kept in view. A tap emits {Activate, entry, Main,
// then Select}; a long press, then Palette.
class EntryListBox : public Tappable {
 public:
  EntryListBox() = default;
  EntryListBox(const EntryListBox&) = delete;
  EntryListBox& operator=(const EntryListBox&) = delete;

  void create(lv_obj_t* parent);
  // The font for each entry's first line (rc::EntryList::setBigHeads: the
  // next size up); null keeps the inherited one.
  void setHeadFont(const lv_font_t* f);
  void sync(const rc::EntryList& m);

  lv_obj_t* obj() const { return box_; }
  lv_obj_t* line1(int i) const { return rows_[i].line1; }
  lv_obj_t* line2(int i) const { return rows_[i].line2; }

  void tapped(lv_obj_t* obj, bool hold) override;

 private:
  struct Row {
    lv_obj_t *row, *line1, *line2;
  };
  void styleHead(lv_obj_t* line1);
  lv_obj_t* box_ = nullptr;
  std::vector<Row> rows_;
  const lv_font_t* head_ = nullptr;
  int sel_ = -1;
};

}  // namespace lvui
