#pragma once

#include <lvgl.h>

#include <vector>

#include "lvui/views/view_util.h"
#include "ui/widgets/list_view.h"

namespace lvui {

// rc::ListView as a scrolling column of one-cell-high rows: "• label  right ".
// The selected row is checked (Role::Selected) while rc::cursorShown(), and
// kept in view. A tap on a row emits {Activate, row, list, then Select}; a
// long press, then Palette (input/touch_resolve).
class ListBox : public Tappable {
 public:
  ListBox() = default;
  ListBox(const ListBox&) = delete;
  ListBox& operator=(const ListBox&) = delete;

  // A full-width column as tall as its rows; size it to scroll.
  void create(lv_obj_t* parent);
  // The list its rows' taps name (rc::ListView::setListId). Default Main.
  void setListId(rc::ListId id) { list_ = id; }
  void sync(const rc::ListView& m);

  lv_obj_t* obj() const { return box_; }
  lv_obj_t* label(int i) const { return rows_[i].label; }
  lv_obj_t* right(int i) const { return rows_[i].right; }

  void tapped(lv_obj_t* obj, bool hold) override;

 private:
  struct Row {
    lv_obj_t *row, *marker, *label, *right;
    bool error;
  };
  lv_obj_t* box_ = nullptr;
  std::vector<Row> rows_;
  rc::ListId list_ = rc::ListId::Main;
  int sel_ = -1;
};

}  // namespace lvui
