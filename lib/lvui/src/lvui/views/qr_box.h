#pragma once

#include <lvgl.h>

#include <string>
#include <string_view>
#include <vector>

namespace lvui {

// A QR code as LinkView draws it from rc::qrToCells: the half-block rows in
// black on white, two cells of margin either side and a row above and below,
// in fixed colours (not the theme's) so phones can scan it. The caller
// places obj(); it is (cols() x rows()) cells of the parent's font.
class QrBox {
 public:
  QrBox() = default;
  QrBox(const QrBox&) = delete;
  QrBox& operator=(const QrBox&) = delete;

  void create(lv_obj_t* parent);
  // Encodes text when it changed. False (and hidden) when it doesn't fit.
  bool sync(std::string_view text);

  lv_obj_t* obj() const { return box_; }
  int cols() const { return cols_; }
  int rows() const { return rows_; }
  // QR cell row i's label.
  lv_obj_t* row(int i) const { return lines_[i]; }

 private:
  lv_obj_t* box_ = nullptr;
  std::vector<lv_obj_t*> lines_;
  std::string text_;
  bool ok_ = false, synced_ = false;
  int cols_ = 0, rows_ = 0;
};

}  // namespace lvui
