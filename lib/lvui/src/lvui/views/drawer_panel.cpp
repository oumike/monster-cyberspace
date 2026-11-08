#include "lvui/views/drawer_panel.h"

#include <cstdint>

#include "lvui/styles.h"
#include "ui/nick_color.h"
#include "util/utf8.h"

namespace lvui {

void DrawerPanel::create(lv_obj_t* parent) {
  panel_ = plainBox(parent);
  addStyle(panel_, Role::Drawer);
  addStyle(panel_, Role::DrawerEdge);
  lv_obj_set_flex_flow(panel_, LV_FLEX_FLOW_COLUMN);
  head_ = plainBox(panel_);
  lv_obj_set_width(head_, lv_pct(100));
  lv_obj_set_height(head_, LV_SIZE_CONTENT);
  header_ = lineLabel(head_);
  addStyle(header_, Role::Dim);
  close_ = lineLabel(head_);
  lv_label_set_text_static(close_, "\u2715");
  addTap(close_, this);
  list_ = plainBox(panel_);
  lv_obj_add_flag(list_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(list_, LV_DIR_VER);
  lv_obj_set_width(list_, lv_pct(100));
  lv_obj_set_flex_grow(list_, 1);
  lv_obj_set_flex_flow(list_, LV_FLEX_FLOW_COLUMN);
  rows_.clear();
  lastW_ = lastH_ = sel_ = -1;
}

void DrawerPanel::setDocked(bool docked) {
  if (docked == docked_) return;
  docked_ = docked;
  lastW_ = -1;
}

// The grid's columns: the edge in column x, text from x + 1.
void DrawerPanel::layout() {
  lv_obj_t* parent = lv_obj_get_parent(panel_);
  lv_obj_update_layout(parent);
  const int w = lv_obj_get_content_width(parent), h = lv_obj_get_content_height(parent);
  if (w == lastW_ && h == lastH_) return;
  lastW_ = w;
  lastH_ = h;
  const Cell c = cellOf(panel_);
  if (c.w <= 0) return;
  const int cols = w / c.w;
  const int width = rc::Drawer::width(cols, docked_);
  inner_ = width - 1;
  lv_obj_set_size(panel_, width * c.w, h);
  lv_obj_align(panel_, LV_ALIGN_TOP_RIGHT, 0, 0);
  lv_obj_set_style_pad_left(panel_, c.w, LV_PART_MAIN);
  lv_obj_set_width(header_, (inner_ - 1) * c.w);
  // Docked, ✕ in the second-last column; its target takes the three last.
  const bool close = docked_ && inner_ >= 4;
  setHidden(close_, !close);
  lv_obj_set_width(close_, c.w);
  lv_obj_align(close_, LV_ALIGN_TOP_RIGHT, -c.w, 0);
  lv_obj_set_ext_click_area(close_, c.w);
}

void DrawerPanel::sync(const rc::Drawer& m) {
  layout();
  const auto& people = m.people();
  setText(header_, U"here \u00b7 " + rc::utf8ToU32(std::to_string(people.size())));
  const int n = static_cast<int>(people.size());
  while (static_cast<int>(rows_.size()) > n) {
    lv_obj_delete(rows_.back().row);
    rows_.pop_back();
  }
  while (static_cast<int>(rows_.size()) < n) {
    Row r;
    r.row = plainBox(list_);
    lv_obj_set_size(r.row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(r.row, LV_FLEX_FLOW_ROW);
    addStyle(r.row, Role::Focused, LV_STATE_CHECKED);
    lv_obj_set_user_data(r.row, reinterpret_cast<void*>(static_cast<intptr_t>(rows_.size())));
    addTap(r.row, this);
    r.name = lineLabel(r.row);
    lv_obj_set_width(r.name, LV_SIZE_CONTENT);
    r.idle = lineLabel(r.row);
    lv_obj_set_width(r.idle, LV_SIZE_CONTENT);
    addStyle(r.idle, Role::Dim);
    rows_.push_back(r);
    sel_ = -1;
  }
  const rc::Theme& t = currentTheme();
  for (int i = 0; i < n; ++i) {
    const rc::Drawer::Person& p = people[i];
    Row& r = rows_[i];
    const int nameMax = inner_ - (p.idle ? 2 : 0);
    setText(r.name, fitCells(rc::utf8ToU32(p.username), nameMax));
    setText(r.idle, p.idle ? std::u32string_view(U" z") : std::u32string_view());
    const bool self = !m.self().empty() && rc::sameUsername(p.username, m.self());
    setTextColor(r.name, rc::nickColor(t, p.username, self));
    setChecked(r.row, i == m.selected() && rc::cursorShown());
  }
  if (n > 0 && m.selected() != sel_) {
    sel_ = m.selected();
    lv_obj_update_layout(list_);
    lv_obj_scroll_to_view(rows_[sel_].row, LV_ANIM_OFF);
  }
}

void DrawerPanel::tapped(lv_obj_t* obj, bool hold) {
  if (obj == close_) {
    rc::KeyEvent e;
    e.action = rc::Action::Activate;
    e.target = static_cast<uint8_t>(rc::ListId::Drawer);
    e.index = rc::Drawer::kCloseIndex;
    emit(e);
    return;
  }
  emit(rowKey(rc::ListId::Drawer, static_cast<int>(reinterpret_cast<intptr_t>(lv_obj_get_user_data(obj))), hold));
}

}  // namespace lvui
