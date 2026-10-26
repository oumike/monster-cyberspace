#include "ui/widgets/drawer.h"

#include <algorithm>

#include "ui/nick_color.h"
#include "util/utf8.h"

namespace rc {

int Drawer::width(TextSize) { return 15; }

void Drawer::setPeople(std::vector<PresenceEntry> people, int64_t nowMs, int64_t idleAfterMs) {
  std::string keep = selectedUsername();
  people_.clear();
  for (const auto& p : people) people_.push_back({p.username, isIdle(p, nowMs, idleAfterMs)});
  sel_ = 0;
  for (size_t i = 0; i < people_.size(); ++i)
    if (people_[i].username == keep) sel_ = static_cast<int>(i);
}

void Drawer::up() {
  if (sel_ > 0) --sel_;
}

void Drawer::down() {
  if (sel_ + 1 < static_cast<int>(people_.size())) ++sel_;
}

std::string Drawer::selectedUsername() const {
  return sel_ < static_cast<int>(people_.size()) ? people_[sel_].username : std::string();
}

void Drawer::render(Grid& g, int top, int bottom, const Theme& t) const {
  const int x = std::max(0, g.cols() - width(TextSize::Compact));
  const int inner = g.cols() - x - 1;
  const int height = bottom - top;
  if (height <= 0 || inner <= 0) return;

  for (int r = 0; r < height; ++r) {
    int row = top + r;
    g.put(x, row, U'\u2502', t.drawerEdge, t.drawerBg);
    for (int c = x + 1; c < g.cols(); ++c) g.put(c, row, U' ', t.fg, t.drawerBg);
  }
  std::u32string head = U"here \u00b7 " + utf8ToU32(std::to_string(people_.size()));
  g.putText(x + 1, top, head, t.dim, t.drawerBg, inner);

  const int listRows = height - 1;
  if (listRows <= 0) return;
  int first = std::max(0, sel_ - listRows + 1);
  for (int r = 0; r < listRows; ++r) {
    int idx = first + r;
    if (idx >= static_cast<int>(people_.size())) break;
    int row = top + 1 + r;
    const Person& p = people_[idx];
    bool sel = idx == sel_;
    uint16_t bg = t.drawerBg;
    uint16_t fg = nickColor(p.username, false);
    for (int c = x + 1; c < g.cols(); ++c) g.put(c, row, U' ', fg, bg);
    std::u32string name = utf8ToU32(p.username);
    int nameMax = inner - (p.idle ? 2 : 0);
    if (static_cast<int>(name.size()) > nameMax)
      name = name.substr(0, std::max(nameMax - 1, 0)) + U"\u2026";
    int n = g.putText(x + 1, row, name, fg, bg, nameMax);
    if (p.idle) g.putText(x + 1 + n, row, U" z", t.dim, bg, 2);
    if (sel) g.outline(x + 1, row, g.cols() - 1, row, t.selectBg);
  }
}

}  // namespace rc
