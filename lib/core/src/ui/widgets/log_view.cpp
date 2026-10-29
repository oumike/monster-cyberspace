#include "ui/widgets/log_view.h"

#include "ui/widgets/scroll_bar.h"

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
        pin_ = lastStart_;
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
  // Lines added or dropped above move every index: keep a held view on the
  // same message line.
  auto anchor = [&](int at, std::string& id, int& off) {
    id.clear();
    if (at < 0 || at >= static_cast<int>(lines_.size())) return;
    int i = at;
    while (i < static_cast<int>(lines_.size()) && lines_[i].divider) ++i;
    if (i == static_cast<int>(lines_.size())) return;
    id = lines_[i].messageId;
    off = at - firstLineOf(id);
  };
  std::string pinId, lastId;
  int pinOff = 0, lastOff = 0;
  anchor(pin_, pinId, pinOff);
  anchor(lastStart_, lastId, lastOff);
  lines_ = std::move(lines);
  auto place = [&](const std::string& id, int off) {
    const int f = id.empty() ? -1 : firstLineOf(id);
    return f < 0 ? -1 : std::max(0, f + off);
  };
  pin_ = place(pinId, pinOff);
  lastStart_ = place(lastId, lastOff);
  if (!selected_.empty() && firstLineOf(selected_) < 0) {
    selected_.clear();
    pin_ = -1;
  }
}

void LogView::unpin() {
  if (pin_ < 0) return;
  if (!cursorShown() && !lines_.empty()) {
    int i = std::min(pin_ + std::max(lastHeight_, 1), static_cast<int>(lines_.size())) - 1;
    while (i > 0 && lines_[i].divider) --i;
    if (!lines_[i].divider) selected_ = lines_[i].messageId;
    pin_ = -1;
  }
}

void LogView::up() {
  unpin();
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
  unpin();
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
  const int barCol = g.cols() - rightReserve - kBarCols;
  const int width = barCol;
  if (height <= 0 || width <= 0) return;

  int start, end;
  if (following()) {
    end = static_cast<int>(lines_.size());
    start = std::max(0, end - height);
  } else {
    int first = firstLineOf(selected_), last = lastLineOf(selected_);
    const int n = static_cast<int>(lines_.size());
    if (pin_ >= 0) {
      pin_ = std::min(pin_, std::max(0, n - height));
      const bool inView = first >= pin_ && last < pin_ + height;
      if (!inView && cursorShown())
        pin_ = first < pin_ ? first : std::min(first, last + 1 - height);
      else if (!inView)
        pin_ = -1;
    }
    if (pin_ >= 0) {
      start = pin_;
    } else {
      end = last + 1;
      start = std::max(0, end - height);
      if (start > first) start = first;
    }
    end = std::min(n, start + height);
  }
  lastStart_ = start;
  lastHeight_ = height;

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
  for (int r = top; r < bottom; ++r) g.put(barCol, r, U' ', t.fg, t.bg);
  drawScrollBar(g, barCol, top, bottom, static_cast<int>(lines_.size()), height, start, t, t.bg);
}

}  // namespace rc
