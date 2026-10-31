#include "ui/widgets/osk.h"

#include <algorithm>

#include "ui/emoji_set.h"
#include "ui/tap_map.h"
#include "ui/text_width.h"

namespace rc {

namespace {

struct Spec {
  OskKind kind;
  char32_t ch;
  std::u32string label;
  int wUnits;
  OskPage to;
};

Spec charKey(char32_t c) { return Spec{OskKind::Char, c, std::u32string(1, c), 1, OskPage::Letters}; }

void addChars(std::vector<Spec>& row, std::u32string_view s) {
  for (char32_t c : s) row.push_back(charKey(c));
}

// A row's keys, plus whether it starts half a unit in.
struct Row {
  bool halfOffset;
  std::vector<Spec> keys;
};

std::vector<Row> pageRows(OskPage p) {
  std::vector<Row> rows(4);
  Row& r0 = rows[0];
  Row& r1 = rows[1];
  Row& r2 = rows[2];
  Row& r3 = rows[3];
  const Spec space{OskKind::Space, 0, U"space", 4, OskPage::Letters};
  const Spec bksp{OskKind::Backspace, 0, U"\u232B", 1, OskPage::Letters};
  const Spec enter{OskKind::Enter, 0, U"enter", 2, OskPage::Letters};
  const Spec hide{OskKind::Hide, 0, U"\u25BC", 1, OskPage::Letters};
  switch (p) {
    case OskPage::Letters:
      addChars(r0.keys, U"qwertyuiop");
      r1.halfOffset = true;
      addChars(r1.keys, U"asdfghjkl");
      r2.keys.push_back(Spec{OskKind::Shift, 0, U"\u21E7", 1, OskPage::Letters});
      addChars(r2.keys, U"zxcvbnm,.");
      r3.keys.push_back(Spec{OskKind::Page, 0, U"?123", 1, OskPage::Symbols});
      r3.keys.push_back(Spec{OskKind::Emoji, 0, std::u32string(1, kTrayEmoji[0]), 1, OskPage::Letters});
      break;
    case OskPage::Symbols:
      addChars(r0.keys, U"1234567890");
      addChars(r1.keys, U"-/:;()$&@\"");
      r2.keys.push_back(Spec{OskKind::Page, 0, U"#+=", 2, OskPage::More});
      addChars(r2.keys, U".,?!'_*+");
      r3.keys.push_back(Spec{OskKind::Page, 0, U"ABC", 2, OskPage::Letters});
      break;
    case OskPage::More:
      addChars(r0.keys, U"[]{}#%^=\\|");
      addChars(r1.keys, U"~<>`\u00A3\u00A5\u00A7\u00B0\u00B7\u2022");
      r2.keys.push_back(Spec{OskKind::Page, 0, U"123", 2, OskPage::Symbols});
      addChars(r2.keys, U"\u2026\u2713\u00B1\u00D7\u00F7\u00BF\u00A1\u00E9");
      r3.keys.push_back(Spec{OskKind::Page, 0, U"ABC", 2, OskPage::Letters});
      break;
  }
  r3.keys.push_back(space);
  r3.keys.push_back(bksp);
  r3.keys.push_back(enter);
  r3.keys.push_back(hide);
  return rows;
}

}  // namespace

int Osk::keyRows(TextSize s) { return s == TextSize::Compact ? 3 : 2; }
int Osk::rows(TextSize s) { return 4 * keyRows(s); }
int Osk::gridRows() { return 4 * 3 + 1; }

void Osk::layout(int cols, TextSize s) {
  cols_ = cols;
  size_ = s;
  keys_.clear();
  lit_ = -1;
  const int unit = cols / 10;
  const int margin = (cols - unit * 10) / 2;
  std::vector<Row> rows = pageRows(page_);
  for (int r = 0; r < int(rows.size()); ++r) {
    int col = margin + (rows[r].halfOffset ? unit / 2 : 0);
    for (const Spec& sp : rows[r].keys) {
      OskKey k;
      k.kind = sp.kind;
      k.ch = sp.ch;
      k.label = sp.label;
      k.to = sp.to;
      k.col = col;
      k.row = r * 3;
      k.w = sp.wUnits * unit;
      k.h = 3;
      keys_.push_back(k);
      col += sp.wUnits * unit;
    }
  }
}

void Osk::reset() {
  page_ = OskPage::Letters;
  shifted_ = false;
  caps_ = false;
}

OskOutput Osk::press(int i, uint32_t nowMs) {
  OskOutput o;
  if (i < 0 || i >= int(keys_.size())) return o;
  const OskKey key = keys_[i];
  switch (key.kind) {
    case OskKind::Char: {
      char32_t ch = key.ch;
      if ((shifted_ || caps_) && ch >= U'a' && ch <= U'z') ch -= 32;
      o.key.action = Action::Char;
      o.key.ch = ch;
      if (shifted_ && !caps_) shifted_ = false;  // one-shot clears after a Char
      break;
    }
    case OskKind::Space:
      o.key.action = Action::Char;
      o.key.ch = U' ';
      break;
    case OskKind::Backspace:
      o.key.action = Action::Backspace;
      break;
    case OskKind::Enter:
      o.key.action = Action::Select;
      break;
    case OskKind::Hide:
      o.hide = true;
      break;
    case OskKind::Emoji:
      o.key.action = Action::Emoji;
      break;
    case OskKind::Shift:
      if (caps_) {
        caps_ = false;
        shifted_ = false;
      } else if (shifted_) {
        if (nowMs - lastShiftMs_ <= kShiftLockMs)
          caps_ = true;  // a second tap within the window locks caps
        shifted_ = false;
      } else {
        shifted_ = true;
        lastShiftMs_ = nowMs;
      }
      break;
    case OskKind::Page:
      page_ = key.to;  // Shift state carries across pages
      layout(cols_, size_);
      break;
  }
  return o;
}

void Osk::touch(int i, uint32_t nowMs) {
  if (i < 0 || i >= int(keys_.size())) return;
  lit_ = i;
  litUntil_ = nowMs + kOskFlashMs;
}

void Osk::render(Grid& g, const Theme& t, uint32_t nowMs) const {
  g.fill(t.bg);
  g.setStacked(true);
  const int rowH = cellSize(size_).h;
  const int band = keyRows(size_) * rowH;
  const int above = (band - rowH) / 2;
  for (int b = 0; b < 4; ++b) {
    g.setRowPad(b * 3, above);
    g.setRowPad(b * 3 + 1, rowH);
    g.setRowPad(b * 3 + 2, band - rowH - above);
  }
  g.setRowPad(4 * 3, 0);
  const int lit = lit_ >= 0 && int32_t(litUntil_ - nowMs) > 0 ? lit_ : -1;
  for (int i = 0; i < int(keys_.size()); ++i) {
    const OskKey& k = keys_[i];
    const bool activeShift = k.kind == OskKind::Shift && (shifted_ || caps_);
    const bool on = activeShift || i == lit;
    const uint16_t bg = on ? t.selectBg : t.inputBg;
    const uint16_t fg = on ? t.selectFg : t.fg;
    const int c1 = k.col + k.w - 1, r1 = k.row + k.h - 1;
    for (int r = k.row; r <= r1; ++r)
      for (int c = k.col; c <= c1; ++c) g.put(c, r, U' ', fg, bg);
    const int tw = textWidth(k.label);
    const int spare = k.w - tw;
    const int lc = k.col + std::max(0, spare) / 2;
    g.putText(lc, k.row + 1, k.label, fg, bg, k.w);
    if (spare > 0 && spare % 2) g.nudge(lc, lc + tw - 1, k.row + 1);
    g.outline(k.col, k.row, c1, r1, t.dim);
    if (activeTaps()) {
      Target tt;
      tt.kind = TargetKind::Osk;
      tt.index = int16_t(i);
      activeTaps()->addCells(k.col, k.row, c1, r1, tt);
    }
  }
}

}  // namespace rc
