#include "ui/widgets/canned_picker.h"

#include "util/utf8.h"

namespace rc {

void CannedPicker::open(const std::vector<std::string>& phrases) {
  phrases_.clear();
  std::vector<ListItem> items;
  for (const std::string& p : phrases) {
    phrases_.push_back(utf8ToU32(p));
    items.push_back(ListItem{phrases_.back(), U""});
  }
  items.push_back(ListItem{U"Emoji\u2026", U""});
  box_.open(U"send", std::move(items));
}

CannedPicker::Out CannedPicker::key(const KeyEvent& k) {
  Out o;
  if (!isOpen()) return o;
  // Left and Right would pick or close the Overlay: here they do nothing.
  if (k.action == Action::Left || k.action == Action::Right) return o;
  const int sel = box_.selected();
  switch (box_.key(k)) {
    case Overlay::Result::Picked:
      if (sel >= 0 && sel < int(phrases_.size())) {
        o.kind = Kind::Send;
        o.text = phrases_[sel];
      } else {
        o.kind = Kind::Emoji;
      }
      break;
    case Overlay::Result::Closed:
      o.kind = Kind::Closed;
      break;
    case Overlay::Result::None:
      break;
  }
  return o;
}

void CannedPicker::render(Grid& g, int top, int bottom, const Theme& t) const {
  if (!isOpen()) return;
  box_.render(g, top, bottom, 0, OverlayPlace::Centre, t);
}

}  // namespace rc
