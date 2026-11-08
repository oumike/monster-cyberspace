#pragma once

#include <lvgl.h>

#include <string>
#include <vector>

#include "lvui/views/row_gestures.h"
#include "lvui/views/view_util.h"
#include "ui/widgets/text_area.h"
#include "ui/widgets/text_view.h"

namespace lvui {

// Wrapped text, one label per screen row, in one of two modes (a box keeps
// the first it is synced with):
// - rc::TextView (read-only, e.g. release notes): its lines as it wraps them
//   (ui/wrap), each in its paragraph's colour and bold, a scroll bar in the
//   last column when they overflow; touch is RowGestures on ListId::Log,
//   items the line indexes.
// - rc::TextArea (the Composer body): on inputBg, hard-wrapped one column
//   short of the box for the scroll bar, the cursor's line kept on screen
//   and its cell in Cursor colours while focused; a tap (or long press) on a
//   row emits {Focus, text position under the finger, field id}.
class TextBox : public RowGestures::Rows, public Tappable {
 public:
  TextBox() = default;
  TextBox(const TextBox&) = delete;
  TextBox& operator=(const TextBox&) = delete;

  // Fills the parent; resize obj() to taste.
  void create(lv_obj_t* parent);
  // Not const: like render(), it lays the text out for the box and clamps
  // the scroll.
  void sync(rc::TextView& m);
  void sync(const rc::TextArea& m, bool focused);

  lv_obj_t* obj() const { return box_; }
  int rows() const { return static_cast<int>(rows_.size()); }
  lv_obj_t* label(int r) const { return rows_[r].label; }
  lv_obj_t* cursor() const { return cursor_; }
  const ScrollBarView& scrollBar() const { return bar_; }

  int itemAt(lv_point_t p) const override;
  int shownItems() const override { return rows(); }
  void tapped(lv_obj_t* obj, bool hold) override;

 private:
  enum class Mode { None, View, Area };
  struct Row {
    lv_obj_t* label;
    std::u32string text;
    uint16_t fg;
    bool bold;
    bool coloured;  // fg set on the label yet
  };
  // Sizes the box in rows and columns, remaking the rows when that changes.
  bool shape();
  void setRow(int r, std::u32string_view text, uint16_t fg, bool bold);

  lv_obj_t *box_ = nullptr, *cursor_ = nullptr;
  ScrollBarView bar_;
  RowGestures touch_;
  std::vector<Row> rows_;
  Mode mode_ = Mode::None;
  int cols_ = -1, top_ = 0;
  Cell cell_{0, 0};
  const lv_font_t* font_ = nullptr;
  // TextArea: the text and width its segments were made for.
  std::u32string areaText_;
  int areaWidth_ = -1;
  std::vector<rc::TextArea::Segment> segs_;
  size_t first_ = 0;
  uint8_t field_ = 0;
  // TextArea: the cursor cell drawn last (row, column, glyph).
  int curRow_ = -1, curCol_ = -1;
  char32_t curCp_ = 0;
};

}  // namespace lvui
