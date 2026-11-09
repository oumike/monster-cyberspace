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
// mentionBg. A row in a chat bubble (rc::ChatStyle::Bubbles) has a rounded box
// behind its spans, filled with rc::bubbleFill (outlined on e-paper), rounded
// only at the bubble's first and last rows. The selected message is outlined
// in selectBg while rc::cursorShown(): round its bubble, if it has one. The last column holds the scroll bar. Keys scroll
// through the model; touch is RowGestures on ListId::Log, items the message
// ordinals.
//
// Painted (the default with RC_LV_LEAN, boards without PSRAM): one object
// draws every row's tint and spans itself, with the same pixels, so a row
// costs no objects; row() and span() are then null.
class LogBox : public RowGestures::Rows, Painter {
 public:
  LogBox() = default;
  LogBox(const LogBox&) = delete;
  LogBox& operator=(const LogBox&) = delete;

  // Whether boxes created from now on are painted.
  static void setPainted(bool on);

  // Fills the parent; resize obj() to taste.
  void create(lv_obj_t* parent);
  void sync(const rc::LogView& m);

  lv_obj_t* obj() const { return box_; }
  bool painted() const { return painted_; }
  int rows() const { return static_cast<int>(rows_.size()); }
  lv_obj_t* row(int r) const { return rows_[r].obj; }
  // The text row r shows (its spans joined), for tests and screenshots.
  std::string rowText(int r) const;
  // The label of span i of row r (null when painted).
  lv_obj_t* span(int r, int i) const { return painted_ ? nullptr : rows_[r].spans[i].label; }
  // Row r's spans as shown, either way.
  int spanCount(int r) const { return rows_[r].used; }
  std::string spanText(int r, int i) const;
  uint16_t spanFg(int r, int i) const;
  bool spanBold(int r, int i) const;
  lv_obj_t* selection() const { return sel_; }
  // The bubble behind row r as drawn; false when it has none.
  struct BubbleShown {
    int col = 0, cols = 0;
    uint16_t fill = 0;
    bool top = false, bottom = false, outlined = false;
  };
  bool bubbleAt(int r, BubbleShown& out) const;
  // The selection outline in cells (x1 > x2 while hidden).
  lv_area_t selectionArea() const { return selArea_; }
  const ScrollBarView& scrollBar() const { return bar_; }

  int itemAt(lv_point_t p) const override;
  int shownItems() const override;

 private:
  struct SpanCell {
    lv_obj_t* label;
    uint16_t fg;
    bool bold;
  };
  // A row's part of a bubble: kind is rc::DisplayLine::Bubble.
  struct Bub {
    uint8_t kind = 0, col = 0, cols = 0;
    bool top = false, bottom = false;
    bool operator==(const Bub& o) const {
      return kind == o.kind && col == o.col && cols == o.cols && top == o.top && bottom == o.bottom;
    }
  };
  static Bub bubOf(const rc::DisplayLine* l);
  struct Row {
    lv_obj_t* obj;
    // Labels made so far; those past `used` hidden (deleted with RC_LOW_MEM).
    std::vector<SpanCell> spans;
    int used;
    bool mention;
    int msg;
    // Painted: its spans, runs_[run, run + used), and its text,
    // text_[text, end); `next` the same in next_ / nextText_ during a sync.
    struct Painted {
      uint16_t run, text, end;
      int used;
      bool mention;
      Bub bub;
    } now, next;
    // Object mode: the box behind the spans, made on first use, and what it shows.
    lv_obj_t* bubble = nullptr;
    Bub bub;
    uint16_t bubFill = 0;
    bool bubOutlined = false;
  };
  // A painted span: NUL-ended text at text_[at].
  struct Run {
    uint16_t at, fg;
    bool bold;
  };
  void shape(int rows, int cols, Cell cell);
  void fill(Row& row, const rc::DisplayLine* l);
  void fillBubble(Row& row, const Bub& b);
  // Painted: a row's spans into next_ / nextText_ (row.next).
  void fillPainted(Row& row, const rc::DisplayLine* l);
  bool samePainted(const Row& row) const;
  void paint(lv_obj_t* box, lv_layer_t* layer) override;

  lv_obj_t *box_ = nullptr, *lines_ = nullptr, *sel_ = nullptr;
  lv_area_t selArea_{0, 0, -1, -1};
  bool painted_ = false;
  // Painted: the spans and text shown, and the next ones while a sync builds them.
  std::vector<Run> runs_, next_;
  std::string text_, nextText_;
  ScrollBarView bar_;
  RowGestures touch_;
  std::vector<Row> rows_;
  int cols_ = -1;
  Cell cell_{0, 0};
  const lv_font_t* font_ = nullptr;
};

}  // namespace lvui
