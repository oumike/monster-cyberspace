#include "ui/widgets/text_view.h"

#include <algorithm>

#include "ui/tap_map.h"
#include "ui/widgets/scroll_bar.h"
#include "ui/wrap.h"

namespace rc {

void TextView::setText(std::u32string text, std::vector<Style> styles) {
  if (text != text_) setText(std::move(text));
  styles_ = std::move(styles);
}

void TextView::setText(std::u32string text) {
  text_ = std::move(text);
  styles_.clear();
  laidOutFor_ = -1;
  lines_.clear();
  top_ = 0;
}

int TextView::maxTop() const { return std::max(0, static_cast<int>(lines_.size()) - rows_); }

void TextView::up() { top_ = std::max(0, top_ - 1); }
void TextView::down() { top_ = std::min(maxTop(), top_ + 1); }
void TextView::pageUp() { top_ = std::max(0, top_ - std::max(1, rows_ - 1)); }
void TextView::pageDown() { top_ = std::min(maxTop(), top_ + std::max(1, rows_ - 1)); }

void TextView::layout(int width) {
  lines_.clear();
  laidOutWidth_ = width;
  size_t start = 0;
  int para = -1;
  while (start <= text_.size()) {
    ++para;
    size_t end = text_.find(U'\n', start);
    if (end == std::u32string::npos) end = text_.size();
    std::u32string_view src(text_.data() + start, end - start);
    start = end + 1;

    size_t lead = src.find_first_not_of(U' ');
    if (lead == std::u32string_view::npos) lead = src.size();
    bool bold = false;
    int indent = 0;
    size_t hashes = lead;
    while (hashes < src.size() && src[hashes] == U'#') ++hashes;
    if (hashes > lead && hashes - lead <= 6 && (hashes == src.size() || src[hashes] == U' ')) {
      src.remove_prefix(std::min(src.size(), hashes + 1));
      bold = true;
    } else if (lead + 1 < src.size() && (src[lead] == U'-' || src[lead] == U'*') && src[lead + 1] == U' ') {
      indent = static_cast<int>(lead) + 2;
    }
    const std::vector<std::u32string> wrapped = wrapText(src, width, width - indent);
    for (size_t i = 0; i < wrapped.size(); ++i)
      lines_.push_back(
          Line{i == 0 ? wrapped[i] : std::u32string(size_t(indent), U' ') + wrapped[i], bold, para});
    if (end == text_.size()) break;
  }
}

void TextView::render(Grid& g, int top, int bottom, int left, int width, const Theme& t) {
  rows_ = std::max(0, bottom - top);
  if (laidOutFor_ != width || laidOutRows_ != rows_) {
    laidOutFor_ = width;
    laidOutRows_ = rows_;
    layout(width);
    if (static_cast<int>(lines_.size()) > rows_ && width > 1) layout(width - 1);
  }
  const bool bar = laidOutWidth_ < width;
  top_ = std::min(top_, maxTop());

  if (activeTaps()) activeTaps()->setList(ListId::Log, -1, static_cast<int>(lines_.size()), false, true);
  for (int r = 0; r < rows_; ++r) {
    const int row = top + r;
    for (int c = 0; c < width; ++c) g.put(left + c, row, U' ', t.fg, t.bg);
    const int idx = top_ + r;
    if (idx < static_cast<int>(lines_.size())) {
      const Line& l = lines_[idx];
      const bool styled = l.para < static_cast<int>(styles_.size());
      const uint16_t fg = styled ? styles_[l.para].fg : t.fg;
      const int n = g.putText(left, row, l.text, fg, t.bg, laidOutWidth_);
      if ((l.bold || (styled && styles_[l.para].bold)) && n > 0) g.embolden(left, left + n - 1, row);
    }
    if (activeTaps()) {
      Target tt;
      tt.kind = TargetKind::Row;
      tt.list = ListId::Log;
      tt.index = static_cast<int16_t>(idx);
      activeTaps()->addCells(left, row, left + width - 1, row, tt);
    }
  }

  if (bar) drawScrollBar(g, left + width - 1, top, top + rows_, static_cast<int>(lines_.size()), rows_, top_, t, t.bg);
}

}  // namespace rc
