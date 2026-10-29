#include "ui/widgets/symbol_table.h"

#include <algorithm>
#include <utility>

#include "ui/emoji_set.h"
#include "ui/tap_map.h"

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
  Action a = k.action == Action::Palette ? Action::Select : k.action;
  if (a == Action::Backspace && backspaceCloses_) a = Action::Back;
  if (linear_ && (a == Action::Up || a == Action::Down)) {
    sel_ = (sel_ + (a == Action::Down ? 1 : count() - 1)) % count();
    return o;
  }
  // A tap on a cell (render's Fixed targets): pick it.
  if (a == Action::Activate && k.target == static_cast<uint8_t>(ListId::Menu)) {
    if (k.index < 0 || k.index >= count()) return o;
    sel_ = k.index;
    KeyEvent pick;
    pick.action = Action::Select;
    return key(pick);
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
  const int extra = commands_.empty() ? 0 : 1;
  // Title, [blank], the rows with a blank row between each, [blank], [commands].
  auto gapsFit = [&](int c) { return 2 * ((n + c - 1) / c) + 2 + extra <= bottom; };
  if (emojiSet) {
    // Emoji sit one blank cell apart with a blank row between rows; on a
    // short screen, more go on a row so the blank rows still fit.
    const int maxCols = std::max(kEmojiCols, std::min(n, (g.cols() - 1) / 3));
    int c = kEmojiCols;
    while (c < maxCols && !gapsFit(c)) ++c;
    emojiCols_ = c;
  }
  const int kc = cols();
  const int rows = (n + kc - 1) / kc;
  const int pitch = 3;
  const int width = emojiSet ? kc * pitch + 1 : kc * pitch + 2;
  const bool gaps = emojiSet && gapsFit(kc);
  const int rowStep = gaps ? 2 : 1;
  const int gridRows = rows * rowStep - (gaps ? 1 : 0);
  const int height = 1 + gridRows + (gaps ? 2 : 0) + extra;
  const int left = std::max(0, (g.cols() - width) / 2);
  const int top = std::max(0, (bottom - height) / 2);
  auto cellTap = [&](int c0, int r0, int c1, int i) {
    if (!activeTaps()) return;
    Target tt;
    tt.kind = TargetKind::Fixed;
    tt.tap.action = Action::Activate;
    tt.tap.index = static_cast<int16_t>(i);
    tt.tap.target = static_cast<uint8_t>(ListId::Menu);
    activeTaps()->addCells(c0, r0, c1, r0, tt);
  };
  if (activeTaps()) {
    // A tap outside the table closes it.
    Target back;
    back.kind = TargetKind::Fixed;
    back.tap.action = Action::Back;
    activeTaps()->addCells(0, 0, g.cols() - 1, g.rows() - 1, back);
  }
  for (int r = 0; r < height; ++r)
    for (int c = 0; c < width; ++c) g.put(left + c, top + r, U' ', t.fg, t.bg);
  for (int c = 0; c < width; ++c) g.put(left + c, top, U' ', t.barFg, t.barBg);
  g.putText(left + 1, top, emojiSet ? U"emoji" : U"symbols", t.barFg, t.barBg, width - 2);
  for (int i = 0; i < n; ++i) {
    const int col = left + 1 + (i % kc) * pitch, row = top + 1 + (gaps ? 1 : 0) + (i / kc) * rowStep;
    const bool sel = i == sel_;
    const bool fill = sel && !t.outlined;
    const uint16_t fg = fill ? t.selectFg : t.fg, bg = fill ? t.selectBg : t.bg;
    if (emojiSet) {
      // An emoji fills two cells, with one blank cell before the next.
      g.putText(col, row, items().substr(i, 1), fg, bg, 2);
      cellTap(col, row, col + 2, i);
      if (sel && t.outlined) g.outline(col, row, col + 1, row, t.selectBg);
    } else {
      // A symbol sits between two spaces.
      for (int c = 0; c < pitch; ++c) g.put(col + c, row, U' ', fg, bg);
      g.putText(col + 1, row, items().substr(i, 1), fg, bg, 1);
      cellTap(col, row, col + pitch - 1, i);
      if (sel && t.outlined) g.outline(col, row, col + pitch - 1, row, t.selectBg);
    }
  }
  if (!commands_.empty()) {
    const int row = top + height - 1;
    const bool sel = sel_ >= n;
    const bool fill = sel && !t.outlined;
    const uint16_t fg = fill ? t.selectFg : t.fg, bg = fill ? t.selectBg : t.bg;
    for (int c = 1; c < width - 1; ++c) g.put(left + c, row, U' ', fg, bg);
    g.putText(left + 2, row, commands_, fg, bg, width - 4);
    cellTap(left + 1, row, left + width - 2, n);
    if (sel && t.outlined) g.outline(left + 1, row, left + width - 2, row, t.selectBg);
  }
  g.outline(left, top, left + width - 1, top + height - 1, t.drawerEdge);
}

}  // namespace rc
