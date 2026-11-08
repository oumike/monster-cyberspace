#include "lvui/views/list_box.h"

#include <cstdint>

#include "lvui/styles.h"

namespace lvui {

void ListBox::create(lv_obj_t* parent) {
  box_ = plainBox(parent);
  lv_obj_add_flag(box_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(box_, LV_DIR_VER);
  lv_obj_set_size(box_, lv_pct(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(box_, LV_FLEX_FLOW_COLUMN);
  rows_.clear();
  sel_ = -1;
}

void ListBox::sync(const rc::ListView& m) {
  const auto& items = m.items();
  const int n = static_cast<int>(items.size());
  while (static_cast<int>(rows_.size()) > n) {
    lv_obj_delete(rows_.back().row);
    rows_.pop_back();
  }
  while (static_cast<int>(rows_.size()) < n) {
    Row r;
    r.row = plainBox(box_);
    lv_obj_set_size(r.row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(r.row, LV_FLEX_FLOW_ROW);
    addStyle(r.row, Role::Item);
    addStyle(r.row, Role::Focused, LV_STATE_CHECKED);
    lv_obj_set_user_data(r.row, reinterpret_cast<void*>(static_cast<intptr_t>(rows_.size())));
    addTap(r.row, this);
    r.marker = lineLabel(r.row);
    lv_obj_set_width(r.marker, 2 * cellOf(r.marker).w);
    r.label = lineLabel(r.row);
    lv_obj_set_flex_grow(r.label, 1);
    r.right = lineLabel(r.row);
    lv_obj_set_width(r.right, LV_SIZE_CONTENT);
    r.error = false;
    rows_.push_back(r);
    sel_ = -1;
  }
  for (int i = 0; i < n; ++i) {
    const rc::ListItem& it = items[i];
    Row& r = rows_[i];
    setText(r.marker, it.marker ? std::u32string_view(U"\u2022") : std::u32string_view());
    setText(r.label, it.label);
    // One cell between the label and the right text, one after it.
    setText(r.right, it.right.empty() ? std::u32string() : U" " + it.right + U" ");
    const bool error = it.tone == rc::ItemTone::Error;
    if (error != r.error) {
      for (lv_obj_t* l : {r.marker, r.label, r.right}) {
        if (error)
          addStyle(l, Role::Error);
        else
          lv_obj_remove_style(l, style(Role::Error), 0);
      }
      r.error = error;
    }
    setChecked(r.row, i == m.selected() && rc::cursorShown());
  }
  if (n > 0 && m.selected() != sel_) {
    sel_ = m.selected();
    lv_obj_update_layout(box_);
    lv_obj_scroll_to_view(rows_[sel_].row, LV_ANIM_OFF);
  }
}

void ListBox::tapped(lv_obj_t* obj, bool hold) {
  emit(rowKey(list_, static_cast<int>(reinterpret_cast<intptr_t>(lv_obj_get_user_data(obj))), hold));
}

}  // namespace lvui
