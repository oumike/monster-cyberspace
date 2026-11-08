#pragma once

#include <lvgl.h>

#include <string>
#include <vector>

#include "lvui/views/view_util.h"
#include "ui/widgets/drawer.h"

namespace lvui {

// rc::Drawer as a panel at the right of its parent, rc::Drawer::width columns
// wide and as tall as the parent: a drawer-edge rule at its left, the dim
// "here · N" header, then a scrolling column of names in their nick colours
// (" z" when idle). The selected name is checked and outlined while
// rc::cursorShown(). Rows emit {Activate, row, Drawer, then Select}; docked,
// the header's ✕ emits {Activate, kCloseIndex, Drawer}.
class DrawerPanel : public Tappable {
 public:
  DrawerPanel() = default;
  DrawerPanel(const DrawerPanel&) = delete;
  DrawerPanel& operator=(const DrawerPanel&) = delete;

  void create(lv_obj_t* parent);
  void setDocked(bool docked);
  void sync(const rc::Drawer& m);

  lv_obj_t* obj() const { return panel_; }
  lv_obj_t* header() const { return header_; }
  lv_obj_t* close() const { return close_; }
  lv_obj_t* name(int i) const { return rows_[i].name; }

  void tapped(lv_obj_t* obj, bool hold) override;

 private:
  struct Row {
    lv_obj_t *row, *name, *idle;
  };
  void layout();

  lv_obj_t *panel_ = nullptr, *head_ = nullptr, *header_ = nullptr, *close_ = nullptr;
  lv_obj_t* list_ = nullptr;
  std::vector<Row> rows_;
  bool docked_ = false;
  int inner_ = 0;  // columns right of the edge
  int lastW_ = -1, lastH_ = -1, sel_ = -1;
};

}  // namespace lvui
