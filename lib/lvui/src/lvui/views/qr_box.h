#pragma once

#include <lvgl.h>

#include <string>
#include <string_view>
#include <vector>

#include "lvui/views/view_util.h"

namespace lvui {

// A QR code as LinkView draws it from rc::qrToCells: the half-block rows in
// black on white, two cells of margin either side and a row above and below,
// in fixed colours (not the theme's) so phones can scan it. The caller
// places obj(); it is (cols() x rows()) cells of the parent's font. The box
// is made with the first sync (obj() is null before).
class QrBox {
 public:
  QrBox() = default;
  QrBox(const QrBox&) = delete;
  QrBox& operator=(const QrBox&) = delete;

  void create(lv_obj_t* parent);
  // Encodes text when it or the cell size changed. False (and hidden) when it doesn't fit.
  bool sync(std::string_view text);

  lv_obj_t* obj() const { return box_; }
  int cols() const { return cols_; }
  int rows() const { return rows_; }
  // QR cell row i's label.
  lv_obj_t* row(int i) const { return lines_[i]; }

 private:
  void make();

  lv_obj_t* parent_ = nullptr;
  int32_t index_ = 0;  // the box's place among parent_'s children
  lv_obj_t* box_ = nullptr;
  std::vector<lv_obj_t*> lines_;
  std::string text_;
  bool ok_ = false, synced_ = false;
  int cols_ = 0, rows_ = 0;
  Cell cell_{0, 0};  // the cell the rows were laid out in
};

}  // namespace lvui
