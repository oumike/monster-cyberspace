#include "ui/widgets/overlay.h"

#include <algorithm>
#include <utility>

#include "ui/button_grid.h"
#include "ui/tap_map.h"
#include "ui/text_width.h"
#include "ui/wrap.h"

namespace rc {

namespace {
bool gConfirmButtons = false;
}  // namespace

void Overlay::setConfirmButtons(bool on) { gConfirmButtons = on; }
bool Overlay::confirmButtons() { return gConfirmButtons; }

void Overlay::open(std::u32string title, std::vector<ListItem> items, int selected) {
  confirm_ = buttons_ = false;
  title_ = std::move(title);
  list_.setListId(ListId::Menu);
  list_.setItems(std::move(items));
  list_.setSelected(selected);
  open_ = true;
}

void Overlay::confirm(std::u32string title, std::u32string action) {
  std::vector<ListItem> items;
  items.push_back(ListItem{U"Cancel", U""});
  items.push_back(ListItem{std::move(action), U"", ItemTone::Error});
  open(std::move(title), std::move(items), 0);
  confirm_ = buttons_ = true;
}

void Overlay::openButtons(std::u32string title, std::vector<ListItem> items) {
  open(std::move(title), std::move(items), 0);
  buttons_ = true;
}

Overlay::Result Overlay::key(const KeyEvent& k) {
  switch (k.action) {
    case Action::Up:
      list_.up();
      return Result::None;
    case Action::Down:
      list_.down();
      return Result::None;
    case Action::Select:
    case Action::Right:
      close();
      return Result::Picked;
    case Action::Back:
    case Action::Left:
      close();
      return Result::Closed;
    case Action::Activate: {
      // Only this menu's rows, and only one still on screen, may move the
      // selection; a stale row picks nothing.
      if (k.target != static_cast<uint8_t>(ListId::Menu)) return Result::None;
      if (k.index < 0 || k.index >= static_cast<int>(list_.size())) return Result::None;
      list_.setSelected(k.index);
      KeyEvent then;
      then.action = k.then;
      return key(then);
    }
    default:
      return Result::None;
  }
}

bool Overlay::renderButtonMenu(Grid& g, int top, int bottom, int rightReserve,
                               const Theme& t) const {
  const int n = static_cast<int>(list_.size());
  int labelW = 0;
  for (const auto& it : list_.items()) labelW = std::max(labelW, textWidth(it.label));
  const int avail = g.cols() - rightReserve - 1;  // a blank column at the left
  const int w = std::min(labelW + 2, avail);      // a space each side of the label
  if (n == 0 || w < 3) return false;
  const int perRow = 2 * w + 1 <= avail ? 2 : 1;
  constexpr int kPitch = kButtonRows + 1;
  const int btnTop = bottom - (n + perRow - 1) / perRow * kPitch + 1;
  if (btnTop - 1 < top) return false;
  // Only the buttons and the title are painted: the screen shows around them.
  const int titleW = std::min(textWidth(title_), avail);
  for (int c = 0; c < titleW; ++c) g.put(1 + c, btnTop - 1, U' ', t.fg, t.bg);
  g.putText(1, btnTop - 1, title_, t.fg, t.bg, titleW);
  if (activeTaps()) activeTaps()->setList(ListId::Menu, list_.selected(), n, false);
  for (int i = 0; i < n; ++i) {
    const int c0 = 1 + (i % perRow) * (w + 1), c1 = c0 + w - 1;
    const int r0 = btnTop + (i / perRow) * kPitch, r1 = r0 + kButtonRows - 1;
    const ListItem& it = list_.items()[i];
    for (int r = r0; r <= r1; ++r)
      for (int c = c0; c <= c1; ++c) g.put(c, r, U' ', t.fg, t.bg);
    g.putText(c0 + 1, r0 + kButtonRows / 2, it.label, it.tone == ItemTone::Error ? t.error : t.fg,
              t.bg, w - 2);
    g.outline(c0, r0, c1, r1, t.dim);
    if (activeTaps()) {
      Target tt;
      tt.kind = TargetKind::Row;
      tt.list = ListId::Menu;
      tt.index = static_cast<int16_t>(i);
      activeTaps()->addCells(c0, r0, c1, r1, tt);
    }
  }
  return true;
}

void Overlay::render(Grid& g, int top, int bottom, int rightReserve, OverlayPlace p,
                     const Theme& t) const {
  if (activeTaps()) {
    Target back;
    back.kind = TargetKind::Fixed;
    back.tap.action = Action::Back;
    activeTaps()->addCells(0, 0, g.cols() - 1, g.rows() - 1, back);
  }
  const int n = static_cast<int>(list_.size());
  if (buttons_ && !confirm_ && gConfirmButtons && renderButtonMenu(g, top, bottom, rightReserve, t))
    return;
  if (confirm_ && gConfirmButtons) {
    // The title wrapped above the buttons, two a row (a confirm's Cancel on
    // the left).
    const int btnRows = (n + 1) / 2;
    const int btnTop = bottom - btnRows * (kButtonRows + 1);
    const ButtonGrid b = btnTop > top ? buttonGrid(g.cols(), btnTop, bottom, n, 2, 0) : ButtonGrid{};
    if (static_cast<int>(b.rects.size()) == n) {
      const int width = std::max(1, g.cols() - 2);
      std::vector<std::u32string> lines = wrapText(title_, width, width);
      const int titleTop = std::max(top, btnTop - 1 - static_cast<int>(lines.size()));
      for (int r = titleTop; r < bottom; ++r)
        for (int c = 0; c < g.cols(); ++c) g.put(c, r, U' ', t.fg, t.bg);
      for (size_t i = 0; i < lines.size() && titleTop + int(i) < btnTop - 1; ++i)
        g.putText(1, titleTop + int(i), lines[i], confirm_ ? t.error : t.fg, t.bg, width);
      if (activeTaps()) activeTaps()->setList(ListId::Menu, list_.selected(), n, false);
      for (size_t k = 0; k < b.rects.size(); ++k) {
        const TileRect& r = b.rects[k];
        const ListItem& it = list_.items()[b.first + k];
        const int inW = r.c1 - r.c0 - 1;
        const int lw = std::min(inW, static_cast<int>(it.label.size()));
        g.putText(r.c0 + 1 + (inW - lw) / 2, r.r0 + kButtonRows / 2, it.label,
                  it.tone == ItemTone::Error ? t.error : t.fg, t.bg, inW);
        g.outline(r.c0, r.r0, r.c1, r.r1, t.dim);
        if (activeTaps()) {
          Target tt;
          tt.kind = TargetKind::Row;
          tt.list = ListId::Menu;
          tt.index = static_cast<int16_t>(b.first + k);
          activeTaps()->addCells(r.c0, r.r0, r.c1, r.r1, tt);
        }
      }
      return;
    }
  }
  switch (p) {
    case OverlayPlace::BottomRight: {
      const int h = std::min(n, bottom - 2);
      const int boxTop = bottom - h - 1;
      const int width = std::min(g.cols() - 2, 30);
      const int left = std::max(1, g.cols() - width - 1 - rightReserve);
      for (int c = 0; c < width; ++c) g.put(left + c, boxTop, U' ', t.barFg, t.barBg);
      g.putText(left + 1, boxTop, title_, t.barFg, t.barBg, width - 2);
      list_.render(g, boxTop + 1, bottom, left, width, t);
      break;
    }
    case OverlayPlace::Centre: {
      const int width = std::min(g.cols() - 2, std::max(20, static_cast<int>(title_.size()) + 2));
      const int left = std::max(1, (g.cols() - width) / 2);
      const int boxTop = std::max(1, (bottom - n - 1) / 2);
      for (int c = 0; c < width; ++c) g.put(left + c, boxTop, U' ', t.barFg, t.barBg);
      g.putText(left + 1, boxTop, title_, t.barFg, t.barBg, width - 2);
      list_.render(g, boxTop + 1, std::min(bottom, boxTop + 1 + n), left, width, t);
      break;
    }
    case OverlayPlace::Bottom: {
      g.putText(1, bottom - 3, title_, t.error, t.bg, g.cols() - 1);
      list_.render(g, bottom - 2, bottom, 1, g.cols() - 2, t);
      break;
    }
  }
  (void)top;  // the placements derive their box from `bottom`
}

}  // namespace rc
