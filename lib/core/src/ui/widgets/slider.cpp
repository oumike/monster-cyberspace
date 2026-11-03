#include "ui/widgets/slider.h"

#include <algorithm>

#include "ui/tap_map.h"

namespace rc {

void Slider::open(std::u32string title, int value, int max, int step, std::u32string zeroLabel) {
  title_ = std::move(title);
  zero_ = std::move(zeroLabel);
  max_ = std::max(1, max);
  step_ = std::clamp(step, 1, max_);
  value_ = std::clamp(value, 0, max_);
  open_ = true;
}

std::u32string Slider::label() const {
  if (value_ == 0) return zero_;
  std::u32string s;
  for (char ch : std::to_string(value_)) s += char32_t(ch);
  return s + U"%";
}

Slider::Result Slider::key(const KeyEvent& k) {
  if (!open_) return Result::None;
  const int before = value_;
  switch (k.action) {
    case Action::Left:
    case Action::Down:
      value_ = std::max(0, value_ - step_);
      break;
    case Action::Right:
    case Action::Up:
      value_ = std::min(max_, value_ + step_);
      break;
    case Action::Activate:
      // A tap on bar segment `index`: the value at its right end.
      if (k.target == uint8_t(ListId::Menu) && k.index >= 0 && k.index < steps())
        value_ = (k.index + 1) * step_;
      break;
    case Action::Select:
      open_ = false;
      return Result::Picked;
    case Action::Back:
      open_ = false;
      return Result::Closed;
    default:
      return Result::None;
  }
  return value_ != before ? Result::Changed : Result::None;
}

void Slider::render(Grid& g, int top, int bottom, const Theme& t) const {
  if (!open_) return;
  const int n = steps();
  // Each step one or two cells wide, with "- " and " +" around the bar.
  const int avail = g.cols() - 2 - 8;
  const int seg = avail >= 2 * n ? 2 : 1;
  const int barW = std::min(avail, seg * n);
  const int w = std::min(g.cols() - 2, std::max(barW + 8, int(title_.size()) + 4));
  const int h = 8;  // border, title, blank, bar, value, blank, buttons, border
  const int c0 = (g.cols() - w) / 2;
  const int r0 = top + std::max(0, (bottom - top - h) / 2);
  if (activeTaps()) {
    // Under everything else: a stray tap does nothing.
    Target none;
    none.kind = TargetKind::Fixed;
    activeTaps()->addCells(0, 0, g.cols() - 1, g.rows() - 1, none);
  }
  for (int r = r0; r < r0 + h && r < bottom; ++r)
    for (int c = c0; c < c0 + w; ++c) g.put(c, r, U' ', t.fg, t.bg);
  g.outline(c0, r0, c0 + w - 1, std::min(bottom - 1, r0 + h - 1), t.dim);
  g.putText(c0 + 2, r0 + 1, title_, t.fg, t.bg, w - 4);

  const int barRow = r0 + 3;
  const int bx = c0 + (w - barW) / 2;
  g.put(bx - 2, barRow, U'-', t.selectBg, t.bg);
  g.put(bx + barW + 1, barRow, U'+', t.selectBg, t.bg);
  const int filled = n ? (value_ / step_) * barW / n : 0;
  for (int c = 0; c < barW; ++c) {
    const bool on = c < filled;
    g.put(bx + c, barRow, on ? U'\u2588' : U'\u2592', on ? t.selectBg : t.dim, t.bg);
  }
  const std::u32string lab = label();
  g.putText(c0 + (w - int(lab.size())) / 2, r0 + 4, lab, t.fg, t.bg, w - 4);

  const int btnRow = r0 + 6;
  const std::u32string cancel = U"[Cancel]", set = U"[Set]";
  const int cx = c0 + 2, sx = c0 + w - 2 - int(set.size());
  g.putText(cx, btnRow, cancel, t.dim, t.bg, int(cancel.size()));
  g.putText(sx, btnRow, set, t.selectBg, t.bg, int(set.size()));

  if (TapMap* m = activeTaps()) {
    auto fixed = [&](int x0, int x1, int row, Action a) {
      Target tt;
      tt.kind = TargetKind::Fixed;
      tt.tap.action = a;
      m->addCells(x0, row, x1, row, tt);
    };
    fixed(bx - 3, bx - 1, barRow, Action::Left);
    fixed(bx + barW, bx + barW + 2, barRow, Action::Right);
    fixed(cx, cx + int(cancel.size()) - 1, btnRow, Action::Back);
    fixed(sx, sx + int(set.size()) - 1, btnRow, Action::Select);
    for (int i = 0; i < n; ++i) {
      const int x0 = bx + i * barW / n, x1 = bx + (i + 1) * barW / n - 1;
      Target tt;
      tt.kind = TargetKind::Row;
      tt.list = ListId::Menu;
      tt.index = int16_t(i);
      m->addCells(x0, barRow - 1, std::max(x0, x1), barRow + 1, tt);
    }
  }
}

}  // namespace rc
