#include "ui/widgets/overlay.h"

#include <algorithm>
#include <utility>

namespace rc {

void Overlay::open(std::u32string title, std::vector<ListItem> items, int selected) {
  title_ = std::move(title);
  list_.setItems(std::move(items));
  list_.setSelected(selected);
  open_ = true;
}

void Overlay::confirm(std::u32string title, std::u32string action) {
  std::vector<ListItem> items;
  items.push_back(ListItem{U"Cancel", U""});
  items.push_back(ListItem{std::move(action), U"", ItemTone::Error});
  open(std::move(title), std::move(items), 0);
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

void Overlay::render(Grid& g, int top, int bottom, int rightReserve, OverlayPlace p,
                     const Theme& t) const {
  const int n = static_cast<int>(list_.size());
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
