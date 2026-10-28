#include "ui/widgets/symbol_table.h"

#include <algorithm>
#include <utility>

#include "ui/emoji_set.h"

namespace rc {

const std::u32string& SymbolTable::symbols() {
  // camillia-mt's tray, roughly by how often a message needs them.
  static const std::u32string s = U"!?.,:;'\"-_/@#&+=()[]{}<>*%$|\\~^`";
  return s;
}

const std::u32string& SymbolTable::emoji() {
  static const std::u32string s(std::begin(kTrayEmoji), std::end(kTrayEmoji));
  return s;
}

void SymbolTable::open(std::u32string commands, Set set, bool linear) {
  set_ = set;
  linear_ = linear;
  commands_ = std::move(commands);
  sel_ = 0;
  open_ = true;
}

int SymbolTable::count() const {
  return static_cast<int>(items().size()) + (commands_.empty() ? 0 : 1);
}

SymbolTable::Out SymbolTable::key(const KeyEvent& k) {
  Out o;
  const bool emojiSet = set_ == Set::Emoji;
  const int n = static_cast<int>(items().size());
  const int kc = cols();
  const int rows = (n + kc - 1) / kc;
  const bool onCommands = sel_ >= n;
  const int col = onCommands ? 0 : sel_ % kc, row = onCommands ? rows : sel_ / kc;
  const int lastRow = commands_.empty() ? rows - 1 : rows;
  auto at = [&](int r, int c) { return r >= rows ? n : std::min(n - 1, r * kc + c); };
  // The Pager's Sym stays armed after opening the table, so its Enter arrives
  // as Sym+Enter (Palette): inside the table it picks.
  const Action a = k.action == Action::Palette ? Action::Select : k.action;
  if (linear_ && (a == Action::Up || a == Action::Down)) {
    sel_ = (sel_ + (a == Action::Down ? 1 : count() - 1)) % count();
    return o;
  }
  if (a == Action::Symbols && !emojiSet) {
    set_ = Set::Emoji;
    sel_ = 0;
    return o;
  }
  switch (a) {
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
    case Action::Select:
      if (!emojiSet || onCommands) open_ = false;
      if (onCommands) {
        o.kind = Kind::Commands;
      } else {
        o.kind = Kind::Insert;
        o.ch = items()[sel_];
      }
      return o;
    case Action::Back:
      open_ = false;
      o.kind = Kind::Closed;
      return o;
    default:
      if (emojiSet || a == Action::Symbols) return o;  // swallowed, or a repeat
      open_ = false;
      o.kind = Kind::PassThrough;
      return o;
  }
}

void SymbolTable::render(Grid& g, int bottom, const Theme& t) const {
  if (!open_) return;
  const bool emojiSet = set_ == Set::Emoji;
  const int n = static_cast<int>(items().size());
  const int kc = cols();
  const int rows = (n + kc - 1) / kc;
  // Emoji get four cells each (two blank between neighbours) and a blank row
  // between rows when the screen is tall enough.
  const int pitch = emojiSet ? 4 : 3;
  const int width = kc * pitch + (emojiSet ? 0 : 2);
  const int extra = commands_.empty() ? 0 : 1;
  // Title, [blank], the rows, [blank], [commands].
  const bool gaps = emojiSet && 1 + 2 * rows + 1 + extra <= bottom;
  const int rowStep = gaps ? 2 : 1;
  const int gridRows = rows * rowStep - (gaps ? 1 : 0);
  const int height = 1 + gridRows + (gaps ? 2 : 0) + extra;
  const int left = std::max(0, (g.cols() - width) / 2);
  const int top = std::max(0, (bottom - height) / 2);
  for (int r = 0; r < height; ++r)
    for (int c = 0; c < width; ++c) g.put(left + c, top + r, U' ', t.fg, t.bg);
  for (int c = 0; c < width; ++c) g.put(left + c, top, U' ', t.barFg, t.barBg);
  g.putText(left + 1, top, emojiSet ? U"emoji" : U"symbols", t.barFg, t.barBg, width - 2);
  for (int i = 0; i < n; ++i) {
    const int col = left + 1 + (i % kc) * pitch, row = top + 1 + (gaps ? 1 : 0) + (i / kc) * rowStep;
    const bool sel = i == sel_;
    const uint16_t fg = sel ? t.selectFg : t.fg, bg = sel ? t.selectBg : t.bg;
    // A symbol sits between two spaces; an emoji in the middle two of four.
    for (int c = 0; c < pitch; ++c) g.put(col + c, row, U' ', fg, bg);
    g.putText(col + 1, row, items().substr(i, 1), fg, bg, emojiSet ? 2 : 1);
  }
  if (!commands_.empty()) {
    const int row = top + height - 1;
    const bool sel = sel_ >= n;
    const uint16_t fg = sel ? t.selectFg : t.fg, bg = sel ? t.selectBg : t.bg;
    for (int c = 1; c < width - 1; ++c) g.put(left + c, row, U' ', fg, bg);
    g.putText(left + 2, row, commands_, fg, bg, width - 4);
  }
  g.outline(left, top, left + width - 1, top + height - 1, t.drawerEdge);
}

}  // namespace rc
