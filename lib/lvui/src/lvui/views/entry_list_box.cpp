#include "lvui/views/entry_list_box.h"

#include <cstdint>

#include "lvui/styles.h"

namespace lvui {

void EntryListBox::create(lv_obj_t* parent) {
  box_ = plainBox(parent);
  lv_obj_add_flag(box_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(box_, LV_DIR_VER);
  lv_obj_set_size(box_, lv_pct(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(box_, LV_FLEX_FLOW_COLUMN);
  rows_.clear();
  sel_ = -1;
}

void EntryListBox::styleHead(lv_obj_t* line1) {
  if (head_)
    lv_obj_set_style_text_font(line1, head_, LV_PART_MAIN);
  else
    lv_obj_remove_local_style_prop(line1, LV_STYLE_TEXT_FONT, LV_PART_MAIN);
  lv_obj_set_height(line1, cellOf(line1).h);
}

void EntryListBox::setHeadFont(const lv_font_t* f) {
  if (f == head_) return;
  head_ = f;
  for (Row& r : rows_) styleHead(r.line1);
}

void EntryListBox::sync(const rc::EntryList& m) {
  const auto& rows = m.rows();
  const int n = static_cast<int>(rows.size());
  while (static_cast<int>(rows_.size()) > n) {
    lv_obj_delete(rows_.back().row);
    rows_.pop_back();
  }
  while (static_cast<int>(rows_.size()) < n) {
    Row r;
    r.row = plainBox(box_);
    lv_obj_set_size(r.row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(r.row, LV_FLEX_FLOW_COLUMN);
    addStyle(r.row, Role::Item);
    addStyle(r.row, Role::Focused, LV_STATE_CHECKED);
    lv_obj_set_user_data(r.row, reinterpret_cast<void*>(static_cast<intptr_t>(rows_.size())));
    addTap(r.row, this);
    r.line1 = lineLabel(r.row);
    lv_obj_set_width(r.line1, lv_pct(100));
    styleHead(r.line1);
    r.line2 = lineLabel(r.row);
    lv_obj_set_width(r.line2, lv_pct(100));
    rows_.push_back(r);
    sel_ = -1;
  }
  for (int i = 0; i < n; ++i) {
    const rc::FeedRow& fr = rows[i];
    Row& r = rows_[i];
    setText(r.line1, fr.line1);
    setText(r.line2, fr.line2);
    setTextColor(r.line1, fr.fg1);
    setTextColor(r.line2, fr.fg2);
    setChecked(r.row, i == m.selected() && rc::cursorShown());
  }
  if (n > 0 && m.selected() != sel_) {
    sel_ = m.selected();
    lv_obj_update_layout(box_);
    lv_obj_scroll_to_view(rows_[sel_].row, LV_ANIM_OFF);
  }
}

void EntryListBox::tapped(lv_obj_t* obj, bool hold) {
  emit(rowKey(rc::ListId::Main, static_cast<int>(reinterpret_cast<intptr_t>(lv_obj_get_user_data(obj))), hold));
}

}  // namespace lvui
