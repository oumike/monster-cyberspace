#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "ui/grid.h"
#include "ui/room_layout.h"
#include "ui/theme.h"

namespace rc {

// Scrollable room log. Following shows the newest lines; otherwise one
// message is selected and the view keeps it on screen.
class LogView {
 public:
  static constexpr int kBarCols = 1;
  using Out = std::function<void(DisplayLine&&)>;
  // Lays out the messages firstId..lastId (an empty id: the trailing lines
  // that have none), with any day divider before the first, passing each
  // line to out in order.
  using Fill = std::function<void(const std::string& firstId, const std::string& lastId, const Out& out)>;
  // Keeps the selected message if it is still present, else follows.
  void setLines(std::vector<DisplayLine> lines);
  // Windowed: the lines stream through addLine(), which keeps only which
  // message each one belongs to, then render() lays out just the messages on
  // screen through fill. keepWindow keeps those between frames; without it
  // each line is drawn as it is laid out and none is kept, so a render holds
  // one message's lines at most. Ends with endLines(), which keeps the
  // selection and a held view as setLines() does.
  void beginLines(Fill fill, bool keepWindow);
  void addLine(const DisplayLine& l);
  void endLines();
  // From following, selects the newest message; then steps to older ones.
  void up();
  // Steps to newer messages; past the newest, returns to following.
  void down();
  // True when the oldest message is selected (or there are none).
  bool atTop() const;
  bool following() const { return selected_.empty(); }
  void follow() {
    selected_.clear();
    pin_ = -1;
  }
  std::string selectedId() const { return selected_; }
  // The number of distinct message ids (dividers aside).
  int messageCount() const;
  // Selects the message at `ordinal` among distinct ids, oldest first; false
  // when out of range. A tap selects this way, so the view holds still: it
  // keeps the last frame's lines while the selection is on screen.
  bool selectMessage(int ordinal);
  // Draws rows [top, bottom), leaving rightReserve columns on the right alone.
  // The last kBarCols columns before those hold the scroll bar, so lines are
  // laid out that much narrower.
  void render(Grid& g, int top, int bottom, int rightReserve, const Theme& t) const;

  // One row of a view, as render() would draw it.
  struct Shown {
    const DisplayLine* line;  // null past the end, or where a windowed fill came up short
    int msg;                  // the message ordinal; -1 for a divider or past the end
    bool selected;            // a line of the selected message
  };
  // For a view drawn without a Grid: picks the lines a view `height` rows
  // tall shows exactly as render() does (following, the selection, a held
  // view) and calls row(r, shown) once for each r in [0, height), not always
  // in order. Returns the first line shown.
  int show(int height, const std::function<void(int, const Shown&)>& row) const;
  // Laid-out lines in all, dividers included (the scroll bar's total).
  int lineTotal() const { return lineCount(); }

 private:
  // The lines [start, end) on screen; from: the first line of start's
  // message. stream: a windowed fill that keeps no lines.
  struct Window {
    int start = 0, end = 0, from = 0;
    bool stream = false;
  };
  Window window(int height) const;
  // Which message a line belongs to (its ordinal; a divider takes the next
  // message's), and whether it is a divider.
  struct LineRef {
    int32_t msg;
    bool divider;
  };

  int lineCount() const { return static_cast<int>(refs_.size()); }
  const std::string& idOf(int line) const { return ids_[refs_[line].msg]; }
  int ordinalOf(const std::string& id) const;
  int firstLineOf(const std::string& id) const;
  int lastLineOf(const std::string& id) const;
  void index(const DisplayLine& l);
  // Notes the held view by message before the lines change, and puts it back.
  void anchor();
  void place();
  // The laid-out line, or null if a windowed fill came up short.
  const DisplayLine* lineAt(int i) const;
  static void drawLine(Grid& g, int row, int width, const Theme& t, const DisplayLine* l);

  // Before a step from a held view without a cursor, selects the message
  // at the view's bottom, as if the view had followed the selection.
  void unpin();

  std::vector<LineRef> refs_;
  std::vector<std::string> ids_;  // per message ordinal
  // Every line, or when windowed and kept those from winFrom_ on.
  mutable std::vector<DisplayLine> lines_;
  mutable int winFrom_ = 0;
  mutable bool winValid_ = false;
  bool windowed_ = false, keepWindow_ = true;
  Fill fill_;
  std::string selected_;
  // The first line shown: lastStart_ from the last frame, pin_ held after a
  // tap (-1: none). With a cursor the held view then scrolls only as far as
  // the selection needs.
  mutable int lastStart_ = -1, lastHeight_ = 0;
  mutable int pin_ = -1;
  // The held view's anchors across a change of lines.
  std::string pinId_, lastId_;
  int pinOff_ = 0, lastOff_ = 0;
};

}  // namespace rc
