#include "ui/widgets/log_view.h"

#include "ui/widgets/scroll_bar.h"

#include <algorithm>

#include "ui/tap_map.h"

namespace rc {

void LogView::index(const DisplayLine& l) {
  if (l.divider) {
    refs_.push_back({static_cast<int32_t>(ids_.size()), true});
    return;
  }
  if (ids_.empty() || l.messageId != ids_.back()) {
    ids_.push_back(l.messageId);
  } else {
    // Dividers inside one message belong to it.
    for (size_t i = refs_.size(); i-- > 0 && refs_[i].divider;) refs_[i].msg = static_cast<int32_t>(ids_.size() - 1);
  }
  refs_.push_back({static_cast<int32_t>(ids_.size() - 1), false});
}

int LogView::ordinalOf(const std::string& id) const {
  for (size_t i = 0; i < ids_.size(); ++i)
    if (ids_[i] == id) return static_cast<int>(i);
  return -1;
}

int LogView::messageCount() const { return static_cast<int>(ids_.size()); }

bool LogView::selectMessage(int ordinal) {
  if (ordinal < 0 || ordinal >= static_cast<int>(ids_.size())) return false;
  selected_ = ids_[ordinal];
  pin_ = lastStart_;
  return true;
}

int LogView::firstLineOf(const std::string& id) const {
  const int ord = ordinalOf(id);
  if (ord < 0) return -1;
  for (int i = 0; i < lineCount(); ++i)
    if (!refs_[i].divider && refs_[i].msg == ord) return i;
  return -1;
}

int LogView::lastLineOf(const std::string& id) const {
  const int ord = ordinalOf(id);
  if (ord < 0) return -1;
  for (int i = lineCount(); i-- > 0;)
    if (!refs_[i].divider && refs_[i].msg == ord) return i;
  return -1;
}

// Lines added or dropped above move every index: keep a held view on the
// same message line.
void LogView::anchor() {
  auto at = [&](int line, std::string& id, int& off) {
    id.clear();
    if (line < 0 || line >= lineCount()) return;
    int i = line;
    while (i < lineCount() && refs_[i].divider) ++i;
    if (i == lineCount()) return;
    id = idOf(i);
    off = line - firstLineOf(id);
  };
  at(pin_, pinId_, pinOff_);
  at(lastStart_, lastId_, lastOff_);
}

void LogView::place() {
  auto at = [&](const std::string& id, int off) {
    const int f = id.empty() ? -1 : firstLineOf(id);
    return f < 0 ? -1 : std::max(0, f + off);
  };
  pin_ = at(pinId_, pinOff_);
  lastStart_ = at(lastId_, lastOff_);
  if (!selected_.empty() && firstLineOf(selected_) < 0) {
    selected_.clear();
    pin_ = -1;
  }
}

void LogView::setLines(std::vector<DisplayLine> lines) {
  anchor();
  refs_.clear();
  ids_.clear();
  for (const auto& l : lines) index(l);
  for (auto& r : refs_) r.msg = std::min<int32_t>(r.msg, std::max<int32_t>(0, int32_t(ids_.size()) - 1));
  lines_ = std::move(lines);
  winFrom_ = 0;
  winValid_ = true;
  windowed_ = false;
  fill_ = nullptr;
  place();
}

void LogView::beginLines(Fill fill, bool keepWindow) {
  anchor();
  std::vector<LineRef>().swap(refs_);
  std::vector<std::string>().swap(ids_);
  std::vector<DisplayLine>().swap(lines_);
  winFrom_ = 0;
  winValid_ = false;
  windowed_ = true;
  keepWindow_ = keepWindow;
  fill_ = std::move(fill);
}

void LogView::addLine(const DisplayLine& l) { index(l); }

void LogView::endLines() {
  // Trailing dividers have no next message: they go with the last.
  for (auto& r : refs_) r.msg = std::min<int32_t>(r.msg, std::max<int32_t>(0, int32_t(ids_.size()) - 1));
  place();
}

const DisplayLine* LogView::lineAt(int i) const {
  const int k = i - winFrom_;
  if (!winValid_ || k < 0 || k >= static_cast<int>(lines_.size())) return nullptr;
  return &lines_[k];
}

void LogView::unpin() {
  if (pin_ < 0) return;
  if (!cursorShown() && lineCount() > 0) {
    int i = std::min(pin_ + std::max(lastHeight_, 1), lineCount()) - 1;
    while (i > 0 && refs_[i].divider) --i;
    if (!refs_[i].divider) selected_ = idOf(i);
    pin_ = -1;
  }
}

void LogView::up() {
  unpin();
  int from = following() ? lineCount() : firstLineOf(selected_);
  for (int i = from - 1; i >= 0; --i) {
    if (!refs_[i].divider && idOf(i) != selected_) {
      selected_ = idOf(i);
      return;
    }
  }
}

void LogView::down() {
  if (following()) return;
  unpin();
  for (int i = lastLineOf(selected_) + 1; i < lineCount(); ++i) {
    if (!refs_[i].divider) {
      selected_ = idOf(i);
      return;
    }
  }
  selected_.clear();
}

bool LogView::atTop() const {
  for (int i = 0; i < lineCount(); ++i)
    if (!refs_[i].divider) return !following() && idOf(i) == selected_;
  return true;
}

LogView::Window LogView::window(int height) const {
  Window w;
  if (following()) {
    w.end = lineCount();
    w.start = std::max(0, w.end - height);
  } else {
    int first = firstLineOf(selected_), last = lastLineOf(selected_);
    const int n = lineCount();
    if (pin_ >= 0) {
      pin_ = std::min(pin_, std::max(0, n - height));
      const bool inView = first >= pin_ && last < pin_ + height;
      if (!inView && cursorShown())
        pin_ = first < pin_ ? first : std::min(first, last + 1 - height);
      else if (!inView)
        pin_ = -1;
    }
    if (pin_ >= 0) {
      w.start = pin_;
    } else {
      w.end = last + 1;
      w.start = std::max(0, w.end - height);
      if (w.start > first) w.start = first;
    }
    w.end = std::min(n, w.start + height);
  }
  lastStart_ = w.start;
  lastHeight_ = height;
  // Windowed: lay out just the messages on screen, with the dividers before
  // them. Kept, into lines_; otherwise each line is drawn as it comes.
  w.stream = windowed_ && !keepWindow_ && w.start < w.end && fill_ && !ids_.empty();
  w.from = w.start;
  if (windowed_ && w.start < w.end) {
    const int fromMsg = refs_[w.start].msg;
    while (w.from > 0 && refs_[w.from - 1].msg == fromMsg) --w.from;
    const bool have = winValid_ && w.from >= winFrom_ && w.end <= winFrom_ + static_cast<int>(lines_.size());
    if (!w.stream && !have && fill_ && !ids_.empty()) {
      std::vector<DisplayLine>().swap(lines_);
      fill_(ids_[fromMsg], ids_[refs_[w.end - 1].msg], [this](DisplayLine&& l) { lines_.push_back(std::move(l)); });
      winFrom_ = w.from;
      winValid_ = true;
    }
  }
  return w;
}

int LogView::show(int height, const std::function<void(int, const Shown&)>& row) const {
  if (height <= 0) return 0;
  const Window w = window(height);
  for (int r = 0; r < height; ++r) {
    const int idx = w.start + r;
    Shown s{nullptr, -1, false};
    if (idx < w.end) {
      const LineRef ref = refs_[idx];
      if (!ref.divider) {
        s.msg = ref.msg;
        s.selected = !following() && ids_[ref.msg] == selected_;
      }
      if (!w.stream) s.line = lineAt(idx);
    }
    // A streamed row comes from the fill below, unless the fill comes up short.
    if (!w.stream || idx >= w.end) row(r, s);
  }
  if (w.stream) {
    int idx = w.from;
    fill_(ids_[refs_[w.start].msg], ids_[refs_[w.end - 1].msg], [&](DisplayLine&& l) {
      if (idx >= w.start && idx < w.end) {
        const LineRef ref = refs_[idx];
        Shown s{&l, ref.divider ? -1 : ref.msg, !ref.divider && !following() && ids_[ref.msg] == selected_};
        row(idx - w.start, s);
      }
      ++idx;
    });
    for (int i = std::max(idx, w.start); i < w.end; ++i) {
      const LineRef ref = refs_[i];
      row(i - w.start, Shown{nullptr, ref.divider ? -1 : ref.msg, !ref.divider && !following() && ids_[ref.msg] == selected_});
    }
  }
  return w.start;
}

void LogView::render(Grid& g, int top, int bottom, int rightReserve, const Theme& t) const {
  const int height = bottom - top;
  const int barCol = g.cols() - rightReserve - kBarCols;
  const int width = barCol;
  if (height <= 0 || width <= 0) return;

  const Window w = window(height);
  const int start = w.start, end = w.end, from = w.from;
  const bool stream = w.stream;

  int selTop = -1, selBottom = -1;
  if (activeTaps()) activeTaps()->setList(ListId::Log, -1, messageCount(), false, true);
  for (int r = 0; r < height; ++r) {
    int row = top + r;
    int idx = start + r;
    if (idx >= end) {
      for (int c = 0; c < width; ++c) g.put(c, row, U' ', t.fg, t.bg);
      continue;
    }
    const LineRef ref = refs_[idx];
    if (activeTaps() && !ref.divider) {
      Target tt;
      tt.kind = TargetKind::Row;
      tt.list = ListId::Log;
      tt.index = static_cast<int16_t>(ref.msg);
      activeTaps()->addCells(0, row, width - 1, row, tt);
    }
    bool sel = !following() && !ref.divider && ids_[ref.msg] == selected_;
    if (sel) {
      if (selTop < 0) selTop = row;
      selBottom = row;
    }
    const DisplayLine* lp = stream ? nullptr : lineAt(idx);
    drawLine(g, row, width, t, lp);
  }
  if (stream) {
    int idx = from;
    fill_(ids_[refs_[start].msg], ids_[refs_[end - 1].msg], [&](DisplayLine&& l) {
      if (idx >= start && idx < end) drawLine(g, top + idx - start, width, t, &l);
      ++idx;
    });
  }
  if (selTop >= 0 && width > 0 && cursorShown()) g.outline(0, selTop, width - 1, selBottom, t.selectBg);
  for (int r = top; r < bottom; ++r) g.put(barCol, r, U' ', t.fg, t.bg);
  drawScrollBar(g, barCol, top, bottom, lineCount(), height, start, t, t.bg);
}

void LogView::drawLine(Grid& g, int row, int width, const Theme& t, const DisplayLine* lp) {
  const uint16_t bg = lp && lp->mentionTint ? t.mentionBg : t.bg;
  int col = 0;
  if (lp) {
    for (const auto& s : lp->spans) {
      const int n = g.putText(col, row, s.text, s.fg, bg, width - col);
      if (s.bold) g.embolden(col, col + n - 1, row);
      col += n;
    }
  }
  for (; col < width; ++col) g.put(col, row, U' ', t.fg, bg);
}

}  // namespace rc
