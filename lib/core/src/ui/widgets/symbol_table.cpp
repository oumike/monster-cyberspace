#include "ui/widgets/symbol_table.h"

#include <algorithm>
#include <utility>

namespace rc {

const std::u32string& SymbolTable::symbols() {
  // camillia-mt's tray, roughly by how often a message needs them.
  static const std::u32string s = U"!?.,:;'\"-_/@#&+=()[]{}<>*%$|\\~^`";
  return s;
}

void SymbolTable::open(std::u32string commands) {
  commands_ = std::move(commands);
  sel_ = 0;
  open_ = true;
}

int SymbolTable::count() const {
  return static_cast<int>(symbols().size()) + (commands_.empty() ? 0 : 1);
}

SymbolTable::Out SymbolTable::key(const KeyEvent& k) {
  Out o;
  const int n = static_cast<int>(symbols().size());
  const int rows = (n + kCols - 1) / kCols;
  const bool onCommands = sel_ >= n;
  const int col = onCommands ? 0 : sel_ % kCols, row = onCommands ? rows : sel_ / kCols;
  const int lastRow = commands_.empty() ? rows - 1 : rows;
  auto at = [&](int r, int c) { return r >= rows ? n : std::min(n - 1, r * kCols + c); };
  switch (k.action) {
    case Action::Left:
      sel_ = (sel_ + count() - 1) % count();
      if (!commands_.empty() && sel_ == n) sel_ = n - 1;  // the arrows skip the commands row
      return o;
    case Action::Right:
      sel_ = (sel_ + 1) % count();
      if (sel_ == n) sel_ = 0;
      return o;
    case Action::Up:
      sel_ = at(row == 0 ? lastRow : row - 1, col);
      return o;
    case Action::Down:
      sel_ = at(row == lastRow ? 0 : row + 1, col);
      return o;
    case Action::Symbols:
      return o;
    case Action::Select:
      open_ = false;
      if (onCommands) {
        o.kind = Kind::Commands;
      } else {
        o.kind = Kind::Insert;
        o.ch = symbols()[sel_];
      }
      return o;
    case Action::Back:
      open_ = false;
      o.kind = Kind::Closed;
      return o;
    default:
      open_ = false;
      o.kind = Kind::PassThrough;
      return o;
  }
}

void SymbolTable::render(Grid& g, int bottom, const Theme& t) const {
  if (!open_) return;
  const int n = static_cast<int>(symbols().size());
  const int rows = (n + kCols - 1) / kCols;
  const int width = kCols * 3 + 2;
  const int height = 1 + rows + (commands_.empty() ? 0 : 1);
  const int left = std::max(0, (g.cols() - width) / 2);
  const int top = std::max(0, (bottom - height) / 2);
  for (int r = 0; r < height; ++r)
    for (int c = 0; c < width; ++c) g.put(left + c, top + r, U' ', t.fg, t.bg);
  for (int c = 0; c < width; ++c) g.put(left + c, top, U' ', t.barFg, t.barBg);
  g.putText(left + 1, top, U"symbols", t.barFg, t.barBg, width - 2);
  for (int i = 0; i < n; ++i) {
    const int col = left + 1 + (i % kCols) * 3, row = top + 1 + i / kCols;
    const bool sel = i == sel_;
    const uint16_t fg = sel ? t.selectFg : t.fg, bg = sel ? t.selectBg : t.bg;
    g.put(col, row, U' ', fg, bg);
    g.put(col + 1, row, symbols()[i], fg, bg);
    g.put(col + 2, row, U' ', fg, bg);
  }
  if (!commands_.empty()) {
    const int row = top + 1 + rows;
    const bool sel = sel_ >= n;
    const uint16_t fg = sel ? t.selectFg : t.fg, bg = sel ? t.selectBg : t.bg;
    for (int c = 1; c < width - 1; ++c) g.put(left + c, row, U' ', fg, bg);
    g.putText(left + 2, row, commands_, fg, bg, width - 4);
  }
  g.outline(left, top, left + width - 1, top + height - 1, t.drawerEdge);
}

}  // namespace rc
