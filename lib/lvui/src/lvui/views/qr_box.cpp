#include "lvui/views/qr_box.h"

#include "lvui/views/view_util.h"
#include "ui/qr_cells.h"

namespace lvui {

void QrBox::create(lv_obj_t* parent) {
  box_ = plainBox(parent);
  setBgColor(box_, 0xFFFF);
  setTextColor(box_, 0x0000);
  setHidden(box_, true);
  synced_ = false;
}

bool QrBox::sync(std::string_view text) {
  if (synced_ && text == text_) return ok_;
  synced_ = true;
  text_.assign(text);
  std::vector<std::u32string> cells;
  ok_ = rc::qrToCells(text, cells) && !cells.empty();
  setHidden(box_, !ok_);
  if (!ok_) {
    cols_ = rows_ = 0;
    return false;
  }
  const Cell c = cellOf(box_);
  rows_ = static_cast<int>(cells.size()) + 2;
  cols_ = static_cast<int>(cells[0].size()) + 4;
  lv_obj_set_size(box_, cols_ * c.w, rows_ * c.h);
  while (lines_.size() < cells.size()) lines_.push_back(lineLabel(box_));
  for (size_t y = 0; y < lines_.size(); ++y) {
    lv_obj_t* l = lines_[y];
    setHidden(l, y >= cells.size());
    if (y >= cells.size()) continue;
    place(l, 2 * c.w, static_cast<int>(1 + y) * c.h, (cols_ - 4) * c.w, c.h);
    setText(l, cells[y]);
    setTextColor(l, 0x0000);
  }
  return true;
}

}  // namespace lvui
