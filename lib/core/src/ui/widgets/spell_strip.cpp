#include "ui/widgets/spell_strip.h"

#include <algorithm>

#include "ui/tap_map.h"
#include "ui/text_width.h"

namespace rc {

namespace {

bool wordChar(char32_t c) { return (c >= U'a' && c <= U'z') || (c >= U'A' && c <= U'Z') || c == U'\''; }

// What ends a word and so gets it checked. Not a newline: in the Composer
// body that is Select, which the strip would otherwise take.
bool boundary(char32_t c) {
  return c == U' ' || c == U',' || c == U'.' || c == U'!' || c == U'?' || c == U';' || c == U':' ||
         c == U')' || c == U'"';
}

}  // namespace

void SpellStrip::open(std::u32string_view text, size_t cursor, const spell::Dict& d) {
  if (cursor < 3 || cursor > text.size() || !boundary(text[cursor - 1])) return;
  const size_t end = cursor - 1;
  size_t start = end;
  while (start > 0 && wordChar(text[start - 1])) --start;
  // Only a word that stands on its own: not the tail of a URL, a number, an
  // @mention or a #room.
  if (start > 0) {
    const char32_t p = text[start - 1];
    if (p != U' ' && p != U'(' && p != U'"' && p != U'\n') return;
  }
  const size_t n = end - start;
  if (n == 0 || n > size_t(spell::kMaxWord)) return;
  char w[spell::kMaxWord + 1];
  for (size_t i = 0; i < n; ++i) w[i] = static_cast<char>(text[start + i]);
  w[n] = '\0';
  if (!spell::shouldCheck(w, n) || spell::known(d, w, n)) return;
  char out[spell::kMaxSuggest][spell::kMaxWord + 1];
  const int found = spell::suggest(d, w, n, out, spell::kMaxSuggest);
  if (found <= 0) return;
  opts_[0].assign(text.substr(start, n));
  for (int i = 0; i < found; ++i) {
    opts_[i + 1].clear();
    for (const char* p = out[i]; *p; ++p) opts_[i + 1].push_back(static_cast<char32_t>(*p));
  }
  count_ = shown_ = found + 1;
  sel_ = 0;
  start_ = start;
}

bool SpellStrip::stillThere(std::u32string_view text) const {
  const std::u32string& cur = opts_[sel_];
  return start_ + cur.size() <= text.size() && text.substr(start_, cur.size()) == cur;
}

void SpellStrip::render(Grid& g, int row, const Theme& t) {
  if (!visible()) return;
  g.fillRow(row, t.inputBg);
  // Each option is " word " with a cell between options.
  auto width = [&](int n) {
    int w = 0;
    for (int i = 0; i < n; ++i) w += textWidth(opts_[i]) + 2 + (i ? 1 : 0);
    return w;
  };
  shown_ = count_;
  while (shown_ > 2 && width(shown_) > g.cols()) --shown_;
  if (sel_ >= shown_) sel_ = 0;  // never: the strip is drawn before it is used
  int col = 0;
  for (int i = 0; i < shown_ && col < g.cols(); ++i) {
    if (i) ++col;
    const int w = textWidth(opts_[i]) + 2;
    const bool sel = i == sel_;
    const bool fill = sel && !t.outlined;
    // The typed word dimmer: it is the "no thanks", not a suggestion.
    const uint16_t fg = fill ? t.selectFg : i == 0 ? t.dim : t.fg;
    const uint16_t bg = fill ? t.selectBg : t.inputBg;
    std::u32string cell = U" " + opts_[i] + U" ";
    g.putText(col, row, cell, fg, bg, g.cols() - col);
    if (sel && t.outlined) g.outline(col, row, std::min(col + w, g.cols()) - 1, row, t.fg);
    if (activeTaps()) {
      Target tt;
      tt.kind = TargetKind::Fixed;
      tt.tap.action = Action::Spell;
      tt.tap.index = static_cast<int16_t>(i);
      activeTaps()->addCells(col, row, std::min(col + w, g.cols()) - 1, row, tt);
    }
    col += w;
  }
}

}  // namespace rc
