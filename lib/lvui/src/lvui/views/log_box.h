#pragma once

#include <lvgl.h>

#include <string>
#include <vector>

#include "lvui/views/row_gestures.h"
#include "lvui/views/view_util.h"
#include "ui/widgets/log_view.h"

namespace lvui {

// rc::LogView (the room log) drawn from its laid-out lines (ui/room_layout):
// one row object per screen row, filled each frame with the lines
// LogView::show picks, so a room of any length holds as many objects as the
// box has rows. A line's spans keep their colours (nick colours, Time, Sys,
// Dim) and bold (the bold face of the box's font); a mention line is tinted
// mentionBg. The selected message is outlined in selectBg while
// rc::cursorShown(). The last column holds the scroll bar. Keys scroll
// through the model; touch is RowGestures on ListId::Log, items the message
// ordinals.
class LogBox : public RowGestures::Rows {
 public:
  LogBox() = default;
  LogBox(const LogBox&) = delete;
  LogBox& operator=(const LogBox&) = delete;

  // Fills the parent; resize obj() to taste.
  void create(lv_obj_t* parent);
  void sync(const rc::LogView& m);

  lv_obj_t* obj() const { return box_; }
  int rows() const { return static_cast<int>(rows_.size()); }
  lv_obj_t* row(int r) const { return rows_[r].obj; }
  // The text row r shows (its spans joined), for tests and screenshots.
  std::string rowText(int r) const;
  // The label of span i of row r.
  lv_obj_t* span(int r, int i) const { return rows_[r].spans[i].label; }
  lv_obj_t* selection() const { return sel_; }
  const ScrollBarView& scrollBar() const { return bar_; }

  int itemAt(lv_point_t p) const override;
  int shownItems() const override;

 private:
  struct SpanCell {
    lv_obj_t* label;
    uint16_t fg;
    bool bold;
  };
  struct Row {
    lv_obj_t* obj;
    // Labels made so far; those past `used` hidden (deleted with RC_LOW_MEM).
    std::vector<SpanCell> spans;
    int used;
    bool mention;
    int msg;
  };
  void shape(int rows, int cols, Cell cell);
  void fill(Row& row, const rc::DisplayLine* l);

  lv_obj_t *box_ = nullptr, *lines_ = nullptr, *sel_ = nullptr;
  ScrollBarView bar_;
  RowGestures touch_;
  std::vector<Row> rows_;
  int cols_ = -1;
  Cell cell_{0, 0};
  const lv_font_t* font_ = nullptr;
};

}  // namespace lvui
