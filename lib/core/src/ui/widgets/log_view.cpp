#include "ui/widgets/log_view.h"

#include <algorithm>

#include "ui/tap_map.h"

namespace rc {

int LogView::ordinalOf(const std::string& id) const {
  int n = -1;
  std::string last;
  bool any = false;
  for (const auto& l : lines_) {
    if (l.divider) continue;
    if (!any || l.messageId != last) {
      ++n;
      last = l.messageId;
      any = true;
    }
    if (l.messageId == id) return n;
  }
  return -1;
}

int LogView::messageCount() const {
  int n = 0;
  std::string last;
  bool any = false;
  for (const auto& l : lines_) {
    if (l.divider) continue;
    if (!any || l.messageId != last) {
      ++n;
      last = l.messageId;
      any = true;
    }
  }
  return n;
}

bool LogView::selectMessage(int ordinal) {
  if (ordinal < 0) return false;
  int n = 0;
  std::string last;
  bool any = false;
  for (const auto& l : lines_) {
    if (l.divider) continue;
    if (!any || l.messageId != last) {
      if (n == ordinal) {
        selected_ = l.messageId;
        return true;
      }
      ++n;
      last = l.messageId;
      any = true;
    }
  }
  return false;
}

int LogView::firstLineOf(const std::string& id) const {
  for (size_t i = 0; i < lines_.size(); ++i)
    if (!lines_[i].divider && lines_[i].messageId == id) return static_cast<int>(i);
  return -1;
}

int LogView::lastLineOf(const std::string& id) const {
  for (size_t i = lines_.size(); i-- > 0;)
    if (!lines_[i].divider && lines_[i].messageId == id) return static_cast<int>(i);
  return -1;
}

void LogView::setLines(std::vector<DisplayLine> lines) {
  lines_ = std::move(lines);
  if (!selected_.empty() && firstLineOf(selected_) < 0) selected_.clear();
}

void LogView::up() {
  int from = following() ? static_cast<int>(lines_.size()) : firstLineOf(selected_);
  for (int i = from - 1; i >= 0; --i) {
    if (!lines_[i].divider && lines_[i].messageId != selected_) {
      selected_ = lines_[i].messageId;
      return;
    }
  }
}

void LogView::down() {
  if (following()) return;
  for (size_t i = lastLineOf(selected_) + 1; i < lines_.size(); ++i) {
    if (!lines_[i].divider) {
      selected_ = lines_[i].messageId;
      return;
    }
  }
  selected_.clear();
}

bool LogView::atTop() const {
  for (const auto& l : lines_)
    if (!l.divider) return !following() && l.messageId == selected_;
  return true;
}

void LogView::render(Grid& g, int top, int bottom, int rightReserve, const Theme& t) const {
  const int height = bottom - top;
  const int width = g.cols() - rightReserve;
  if (height <= 0 || width <= 0) return;

  int start, end;
  if (following()) {
    end = static_cast<int>(lines_.size());
    start = std::max(0, end - height);
  } else {
    int first = firstLineOf(selected_), last = lastLineOf(selected_);
    end = last + 1;
    start = std::max(0, end - height);
    if (start > first) start = first;
    end = std::min(static_cast<int>(lines_.size()), start + height);
  }

  int selTop = -1, selBottom = -1;
  if (activeTaps()) activeTaps()->setList(ListId::Log, -1, messageCount(), false, true);
  for (int r = 0; r < height; ++r) {
    int row = top + r;
    int idx = start + r;
    if (idx >= end) {
      for (int c = 0; c < width; ++c) g.put(c, row, U' ', t.fg, t.bg);
      continue;
    }
    const DisplayLine& l = lines_[idx];
    if (activeTaps() && !l.divider) {
      Target tt;
      tt.kind = TargetKind::Row;
      tt.list = ListId::Log;
      tt.index = static_cast<int16_t>(ordinalOf(l.messageId));
      activeTaps()->addCells(0, row, width - 1, row, tt);
    }
    bool sel = !following() && !l.divider && l.messageId == selected_;
    if (sel) {
      if (selTop < 0) selTop = row;
      selBottom = row;
    }
    uint16_t bg = l.mentionTint ? t.mentionBg : t.bg;
    int col = 0;
    for (const auto& s : l.spans) {
      const int n = g.putText(col, row, s.text, s.fg, bg, width - col);
      if (s.bold) g.embolden(col, col + n - 1, row);
      col += n;
    }
    for (; col < width; ++col) g.put(col, row, U' ', t.fg, bg);
  }
  if (selTop >= 0 && width > 0 && cursorShown()) g.outline(0, selTop, width - 1, selBottom, t.selectBg);
}

}  // namespace rc
