#include "lvui/views/button_rows.h"

#include <algorithm>

#include "lvui/styles.h"
#include "ui/emoji_set.h"
#include "ui/text_width.h"
#include "util/utf8.h"

namespace lvui {

namespace {

int parentWidth(lv_obj_t* obj) {
  lv_obj_t* p = lv_obj_get_parent(obj);
  const int w = p ? lv_obj_get_width(p) : 0;
  return w > 0 ? w : lv_display_get_horizontal_resolution(lv_obj_get_display(obj));
}

int parentHeight(lv_obj_t* obj) {
  lv_obj_t* p = lv_obj_get_parent(obj);
  const int h = p ? lv_obj_get_height(p) : 0;
  return h > 0 ? h : lv_display_get_vertical_resolution(lv_obj_get_display(obj));
}

void setBorder(lv_obj_t* obj, int width, uint16_t rgb565) {
  if (lv_obj_get_style_border_width(obj, LV_PART_MAIN) != width)
    lv_obj_set_style_border_width(obj, width, LV_PART_MAIN);
  if (width > 0) lv_obj_set_style_border_color(obj, color(rgb565), LV_PART_MAIN);
}

bool sameLayout(const rc::SymbolTable::Layout& a, const rc::SymbolTable::Layout& b) {
  return a.left == b.left && a.top == b.top && a.width == b.width && a.h == b.h && a.cols == b.cols &&
         a.rows == b.rows && a.pitch == b.pitch && a.rowStep == b.rowStep && a.ew == b.ew && a.eh == b.eh &&
         a.shown == b.shown && a.scroll == b.scroll && a.big == b.big && a.gaps == b.gaps &&
         a.scrolls == b.scrolls;
}

}  // namespace

// --- SpellBar ---------------------------------------------------------------

void SpellBar::create(lv_obj_t* parent) {
  bar_ = plainBox(parent);
  lv_obj_set_width(bar_, lv_pct(100));
  lv_obj_set_height(bar_, cellOf(bar_).h);
  for (lv_obj_t*& o : opts_) o = nullptr;
  setHidden(bar_, true);
  count_ = -1;
}

void SpellBar::sync(rc::SpellStrip& strip) {
  setHidden(bar_, !strip.visible());
  if (!strip.visible()) {
    count_ = -1;
    return;
  }
  // The options are made the first time there are any: boards without
  // spell check (and most trees) never show the bar.
  if (!opts_[0]) {
    for (lv_obj_t*& o : opts_) {
      o = lineLabel(bar_);
      addTap(o, this);
      lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
    }
  }
  const Cell c = cellOf(bar_);
  if (c.w <= 0) return;
  const int cols = parentWidth(bar_) / c.w;
  const int shown = strip.fit(cols);
  bool same = count_ == strip.count() && shown_ == shown && sel_ == strip.selected() && cols_ == cols &&
              theme_ == themeSerial();
  for (int i = 0; same && i < shown; ++i) same = last_[i] == strip.option(i);
  if (same) return;
  count_ = strip.count();
  shown_ = shown;
  sel_ = strip.selected();
  cols_ = cols;
  theme_ = themeSerial();

  const rc::Theme& t = currentTheme();
  setBgColor(bar_, t.inputBg);
  int col = 0;
  for (int i = 0; i < kMax; ++i) {
    lv_obj_t* o = opts_[i];
    const bool on = i < shown && col + (i ? 1 : 0) < cols;
    setHidden(o, !on);
    if (!on) continue;
    if (i) ++col;
    last_[i] = strip.option(i);
    const int w = rc::textWidth(last_[i]) + 2;
    const bool sel = i == sel_;
    const bool fill = sel && !t.outlined;
    place(o, col * c.w, 0, std::min(w, cols - col) * c.w, c.h);
    setText(o, U" " + last_[i] + U" ");
    setTextColor(o, fill ? t.selectFg : i == 0 ? t.dim : t.fg);
    setBgColor(o, fill ? t.selectBg : t.inputBg);
    // Outlined themes: the grid outlines the selection in fg, not selectBg.
    setBorder(o, sel && t.outlined ? 1 : 0, t.fg);
    col += w;
  }
}

void SpellBar::tapped(lv_obj_t* obj, bool) {
  for (int i = 0; i < shown_; ++i) {
    if (opts_[i] != obj) continue;
    rc::KeyEvent e;
    e.action = rc::Action::Spell;
    e.index = static_cast<int16_t>(i);
    emit(e);
    return;
  }
}

// --- SymbolBox --------------------------------------------------------------

void SymbolBox::create(lv_obj_t* parent) {
  root_ = plainBox(parent);
  lv_obj_set_size(root_, lv_pct(100), lv_pct(100));
  addTap(root_, this);
  frame_ = plainBox(root_);
  addStyle(frame_, Role::Screen);
  // The outline goes over the title row, as the grid draws it last.
  lv_obj_set_style_border_post(frame_, true, LV_PART_MAIN);
  titleBar_ = plainBox(frame_);
  title_ = lineLabel(titleBar_);
  commands_ = lineLabel(frame_);
  addStyle(commands_, Role::Item);
  addStyle(commands_, Role::Selected, LV_STATE_CHECKED);
  sbHost_ = plainBox(frame_);
  bar_.create(sbHost_);
  setHidden(root_, true);
  drawn_ = false;
}

// The item labels and their text: on a new set, or a switch between big and
// small emoji.
void SymbolBox::rebuild(const rc::SymbolTable& st, bool big) {
  const std::u32string& items = st.items();
  const int n = static_cast<int>(items.size());
  while (static_cast<int>(cells_.size()) < n) {
    lv_obj_t* l = lineLabel(frame_);
    addStyle(l, Role::Item);
    addStyle(l, Role::Selected, LV_STATE_CHECKED);
    cells_.push_back(l);
  }
  for (int i = 0; i < static_cast<int>(cells_.size()); ++i) {
    lv_obj_t* l = cells_[i];
    if (i >= n) {
      setHidden(l, true);
      continue;
    }
    std::u32string s;
    if (big) {
      for (int r = 0; r < rc::kTrayBigRows; ++r) {
        if (r) s += U'\n';
        for (int c = 0; c < rc::kTrayBigCols; ++c) s += rc::trayBigPiece(i, r, c);
      }
    } else if (st.set() == rc::SymbolTable::Set::Emoji) {
      s = items.substr(i, 1);
    } else {
      s = U" " + items.substr(i, 1) + U" ";
    }
    setText(l, s);
  }
}

void SymbolBox::sync(const rc::SymbolTable& st) {
  setHidden(root_, !st.isOpen());
  if (!st.isOpen()) {
    drawn_ = false;
    return;
  }
  const Cell c = cellOf(root_);
  if (c.w <= 0 || c.h <= 0) return;
  const int gridCols = parentWidth(root_) / c.w, gridRows = parentHeight(root_) / c.h;
  const rc::SymbolTable::Layout L = st.layout(gridCols, gridRows - 1);
  const bool emoji = st.set() == rc::SymbolTable::Set::Emoji;
  const int n = static_cast<int>(st.items().size());
  if (drawn_ && sameLayout(L, lay_) && emoji == emoji_ && L.big == big_ && st.selected() == sel_ &&
      c.w == cw_ && c.h == ch_ && gridCols == gridCols_ && gridRows == gridRows_ &&
      theme_ == themeSerial() && st.commands() == commandsText_)
    return;
  if (!drawn_ || emoji != emoji_ || L.big != big_ || n != n_) rebuild(st, L.big);
  drawn_ = true;
  n_ = n;
  lay_ = L;
  emoji_ = emoji;
  big_ = L.big;
  sel_ = st.selected();
  cw_ = c.w;
  ch_ = c.h;
  gridCols_ = gridCols;
  gridRows_ = gridRows;
  commandsText_ = st.commands();

  const rc::Theme& t = currentTheme();
  if (theme_ != themeSerial()) {
    theme_ = themeSerial();
    lv_obj_set_style_border_color(frame_, color(t.drawerEdge), LV_PART_MAIN);
    lv_obj_set_style_border_width(frame_, 1, LV_PART_MAIN);
    setBgColor(titleBar_, t.barBg);
    setTextColor(title_, t.barFg);
  }
  place(frame_, L.left * c.w, L.top * c.h, L.width * c.w, L.h * c.h);
  place(titleBar_, 0, 0, L.width * c.w, c.h);
  place(title_, c.w, 0, std::max(0, L.width - 2) * c.w, c.h);
  setText(title_, emoji ? std::u32string(U"emoji") : std::u32string(U"symbols"));

  const int first = L.scroll * L.cols, end = std::min(n, (L.scroll + L.shown) * L.cols);
  const int ew = L.big ? L.ew : emoji ? 2 : L.pitch;
  for (int i = 0; i < n; ++i) {
    lv_obj_t* l = cells_[i];
    const bool on = i >= first && i < end;
    setHidden(l, !on);
    if (!on) continue;
    place(l, (L.col(i) - L.left) * c.w, (L.row(i) - L.top) * c.h, ew * c.w, L.eh * c.h);
    setChecked(l, i == sel_);
  }
  const bool cmds = !commandsText_.empty();
  setHidden(commands_, !cmds);
  if (cmds) {
    place(commands_, c.w, (L.h - 1) * c.h, std::max(0, L.width - 2) * c.w, c.h);
    setText(commands_, U" " + commandsText_);
    setChecked(commands_, sel_ >= n);
  }
  place(sbHost_, 0, c.h, L.width * c.w, L.shown * L.eh * c.h);
  bar_.sync(L.width * c.w, c, L.scrolls ? L.shown * L.eh : 0, L.rows, L.shown, L.scroll);
}

// The item under p as the tap map registers it: a symbol's three cells, an
// emoji's cells and the blank one after, the commands row (n_); -1 for none.
int SymbolBox::hit(lv_point_t p) const {
  if (!drawn_ || cw_ <= 0 || ch_ <= 0) return -1;
  lv_area_t a;
  lv_obj_get_coords(root_, &a);
  if (p.x < a.x1 || p.y < a.y1) return -1;
  const int col = (p.x - a.x1) / cw_, row = (p.y - a.y1) / ch_;
  const rc::SymbolTable::Layout& L = lay_;
  const int end = std::min(n_, (L.scroll + L.shown) * L.cols);
  for (int i = L.scroll * L.cols; i < end; ++i) {
    const int c0 = L.col(i), r0 = L.row(i);
    const int c1 = L.big ? c0 + L.ew : emoji_ ? c0 + 2 : c0 + L.pitch - 1;
    const int r1 = r0 + L.eh - 1;
    if (col >= c0 && col <= c1 && row >= r0 && row <= r1) return i;
  }
  if (!commandsText_.empty() && row == L.top + L.h - 1 && col >= L.left + 1 && col <= L.left + L.width - 2)
    return n_;
  return -1;
}

void SymbolBox::tapped(lv_obj_t*, bool) {
  const int i = hit(tapPoint());
  rc::KeyEvent e;
  if (i < 0) {
    e.action = rc::Action::Back;
  } else {
    e.action = rc::Action::Activate;
    e.index = static_cast<int16_t>(i);
    e.target = static_cast<uint8_t>(rc::ListId::Menu);
  }
  emit(e);
}

}  // namespace lvui
