#include "lvui/views/slider_box.h"

#include <algorithm>

#include "lvui/styles.h"
#include "ui/text_width.h"

namespace lvui {

namespace {

const std::u32string kCancel = U"[Cancel]", kSet = U"[Set]";

std::u32string repeat(char32_t c, int n) { return std::u32string(static_cast<size_t>(std::max(n, 0)), c); }

}  // namespace

void SliderBox::create(lv_obj_t* parent) {
  backdrop_ = plainBox(parent);
  lv_obj_set_size(backdrop_, lv_pct(100), lv_pct(100));
  addTap(backdrop_, this);
  box_ = plainBox(backdrop_);
  lv_obj_add_flag(backdrop_, LV_OBJ_FLAG_HIDDEN);
  built_ = false;
}

lv_obj_t* SliderBox::cellLabel(int col, int row) {
  lv_obj_t* l = lineLabel(box_);
  lv_obj_set_pos(l, col * cell_.w, row * cell_.h);
  return l;
}

int SliderBox::dotX(int i) const {
  const int lx = 2;
  return n_ > 1 ? lx + i * (lineW_ - 1) / (n_ - 1) : lx + lineW_ / 2;
}

// The geometry of Slider::render / renderStops, box-relative.
void SliderBox::build(const rc::Slider& m, int cols, int rows) {
  lv_obj_clean(box_);
  dots_.clear();
  names_.clear();
  minus_ = plus_ = filled_ = empty_ = value_ = line_ = nullptr;
  choices_ = !m.names().empty();
  n_ = m.steps();
  const int title = static_cast<int>(m.title().size());
  if (choices_) {
    int namesW = 2 * (n_ - 1);
    for (const std::u32string& nm : m.names()) namesW += static_cast<int>(nm.size());
    lineW_ = std::min(cols - 6, std::max({37, namesW, title}));
    if (n_ > 1) lineW_ -= (lineW_ - 1) % (n_ - 1);
    w_ = lineW_ + 4;
  } else {
    const int avail = cols - 2 - 8;
    const int seg = avail >= 2 * n_ ? 2 : 1;
    barW_ = std::min(avail, seg * n_);
    w_ = std::min(cols - 2, std::max(barW_ + 8, title + 4));
    bx_ = (w_ - barW_) / 2;
  }
  h_ = 8;  // border, title, blank, bar or line, value or names, blank, buttons, border
  c0_ = (cols - w_) / 2;
  r0_ = std::max(0, (rows - h_) / 2);
  lv_obj_set_pos(box_, c0_ * cell_.w, r0_ * cell_.h);
  lv_obj_set_size(box_, w_ * cell_.w, std::min(h_, rows - r0_) * cell_.h);

  title_ = cellLabel(2, 1);
  setText(title_, std::u32string_view(m.title()).substr(0, rc::fitChars(m.title(), w_ - 4)));
  if (choices_) {
    line_ = cellLabel(2, 3);
    setText(line_, repeat(U'\u2500', lineW_));
    for (int i = 0; i < n_; ++i) {
      dots_.push_back(cellLabel(dotX(i), 3));
      const std::u32string& nm = m.names()[i];
      const int len = static_cast<int>(nm.size());
      const int x = i == 0 ? dotX(i) : i == n_ - 1 ? dotX(i) - len + 1 : dotX(i) - len / 2;
      names_.push_back(cellLabel(x, 4));
      setText(names_.back(), nm);
    }
  } else {
    minus_ = cellLabel(bx_ - 2, 3);
    setText(minus_, std::u32string(U"-"));
    plus_ = cellLabel(bx_ + barW_ + 1, 3);
    setText(plus_, std::u32string(U"+"));
    filled_ = cellLabel(bx_, 3);
    empty_ = cellLabel(bx_, 3);
    value_ = cellLabel(0, 4);
  }
  cancel_ = cellLabel(2, 6);
  setText(cancel_, kCancel);
  set_ = cellLabel(w_ - 2 - static_cast<int>(kSet.size()), 6);
  setText(set_, kSet);
  // The outline over the edge cells, as Grid::outline draws it: a child, so
  // a border doesn't push the cells in.
  frame_ = plainBox(box_);
  lv_obj_set_size(frame_, lv_pct(100), lv_pct(100));
  lv_obj_set_style_border_width(frame_, 1, LV_PART_MAIN);
  lv_obj_set_style_border_opa(frame_, LV_OPA_COVER, LV_PART_MAIN);
  shown_ = -1;
}

void SliderBox::update(const rc::Slider& m) {
  const rc::Theme& t = currentTheme();
  if (shown_ == m.value() && bg_ == t.bg && fg_ == t.fg && dim_ == t.dim && sel_ == t.selectBg) return;
  shown_ = m.value();
  bg_ = t.bg;
  fg_ = t.fg;
  dim_ = t.dim;
  sel_ = t.selectBg;
  setBgColor(box_, t.bg);
  lv_obj_set_style_border_color(frame_, color(t.dim), LV_PART_MAIN);
  setTextColor(title_, t.fg);
  setTextColor(cancel_, t.dim);
  setTextColor(set_, t.selectBg);
  if (choices_) {
    setTextColor(line_, t.dim);
    for (int i = 0; i < n_; ++i) {
      const bool on = i == m.value();
      setText(dots_[i], on ? std::u32string_view(U"\u25CF") : std::u32string_view(U"\u25CB"));
      setTextColor(dots_[i], on ? t.selectBg : t.dim);
      // Over the line's cell, as the grid's dot replaces it.
      setBgColor(dots_[i], t.bg);
      setTextColor(names_[i], on ? t.fg : t.dim);
    }
    return;
  }
  setTextColor(minus_, t.selectBg);
  setTextColor(plus_, t.selectBg);
  const int filled = n_ ? (m.value() / std::max(m.step(), 1)) * barW_ / n_ : 0;
  setText(filled_, repeat(U'\u2588', filled));
  setTextColor(filled_, t.selectBg);
  setText(empty_, repeat(U'\u2592', barW_ - filled));
  setTextColor(empty_, t.dim);
  lv_obj_set_x(empty_, (bx_ + filled) * cell_.w);
  const std::u32string lab = m.label();
  setText(value_, std::u32string_view(lab).substr(0, rc::fitChars(lab, w_ - 4)));
  setTextColor(value_, t.fg);
  lv_obj_set_x(value_, (w_ - static_cast<int>(lab.size())) / 2 * cell_.w);
}

void SliderBox::sync(const rc::Slider& m) {
  setHidden(backdrop_, !m.isOpen());
  if (!m.isOpen()) return;
  lv_obj_update_layout(backdrop_);
  const Cell cell = cellOf(backdrop_);
  if (cell.w <= 0 || cell.h <= 0) return;
  const int cols = lv_obj_get_content_width(backdrop_) / cell.w;
  const int rows = lv_obj_get_content_height(backdrop_) / cell.h;
  if (!built_ || cols != cols_ || rows != rows_ || cell.w != cell_.w || cell.h != cell_.h || m.steps() != steps_ ||
      m.step() != step_ || m.title() != titleText_ || m.names() != nameTexts_) {
    built_ = true;
    cols_ = cols;
    rows_ = rows;
    cell_ = cell;
    steps_ = m.steps();
    step_ = m.step();
    titleText_ = m.title();
    nameTexts_ = m.names();
    build(m, cols, rows);
  }
  update(m);
}

void SliderBox::tapped(lv_obj_t*, bool hold) {
  if (!built_ || cell_.w <= 0 || cell_.h <= 0) return;
  lv_area_t a;
  lv_obj_get_content_coords(backdrop_, &a);
  const lv_point_t p = tapPoint();
  if (p.x < a.x1 || p.y < a.y1) return;
  // Box-relative cells.
  const int c = (p.x - a.x1) / cell_.w - c0_, r = (p.y - a.y1) / cell_.h - r0_;
  rc::KeyEvent e;
  auto fixed = [&](rc::Action act) {
    e.action = act;
    emit(e);
  };
  // As the tap map: the last target registered wins.
  if (r >= 2 && r <= 4) {
    for (int i = 0; i < n_; ++i) {
      int x0, x1;
      if (choices_) {
        x0 = i == 0 ? 2 : (dotX(i - 1) + dotX(i)) / 2 + 1;
        x1 = i == n_ - 1 ? 2 + lineW_ - 1 : (dotX(i) + dotX(i + 1)) / 2;
      } else {
        x0 = bx_ + i * barW_ / n_;
        x1 = std::max(x0, bx_ + (i + 1) * barW_ / n_ - 1);
      }
      if (c >= x0 && c <= x1) return emit(rowKey(rc::ListId::Menu, i, hold));
    }
  }
  const int sx = w_ - 2 - static_cast<int>(kSet.size());
  if (r == 6 && c >= sx && c < sx + static_cast<int>(kSet.size())) return fixed(rc::Action::Select);
  if (r == 6 && c >= 2 && c < 2 + static_cast<int>(kCancel.size())) return fixed(rc::Action::Back);
  if (choices_ || r != 3) return;
  if (c >= bx_ + barW_ && c <= bx_ + barW_ + 2) return fixed(rc::Action::Right);
  if (c >= bx_ - 3 && c <= bx_ - 1) return fixed(rc::Action::Left);
}

}  // namespace lvui
