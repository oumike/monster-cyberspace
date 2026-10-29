#include "screens/button_pad.h"

#include <algorithm>

#include "hal/board.h"
#include "ui/button_grid.h"
#include "ui/tap_map.h"

namespace rc {

bool ButtonPad::active() { return board::hasTouch() && !board::hasKeys(); }

int ButtonPad::columns() { return board::panelWidth() >= 800 ? 3 : 2; }

int ButtonPad::key(const KeyEvent& k) {
  constexpr int kPitch = kButtonRows + 1;
  switch (k.action) {
    case Action::Up:
      if (--(dragSteps_ = std::min(dragSteps_, 0)) <= -kPitch) {
        dragSteps_ = 0;
        --scroll_;  // clamped in render
      }
      break;
    case Action::Down:
      if (++(dragSteps_ = std::max(dragSteps_, 0)) >= kPitch) {
        dragSteps_ = 0;
        ++scroll_;
      }
      break;
    case Action::Activate:
      if (k.target == uint8_t(ListId::Main) && k.then == Action::Select) return k.index;
      break;
    default:
      break;
  }
  return -1;
}

void ButtonPad::render(Grid& g, const Theme& t, const std::vector<ListItem>& items, int top,
                       int bottom, bool keepRight, int cols) {
  const int n = int(items.size());
  const ButtonGrid b = buttonGrid(g.cols(), top, bottom, n, cols > 0 ? cols : columns(), scroll_);
  scroll_ = b.scrollRow;
  if (activeTaps()) activeTaps()->setList(ListId::Main, -1, n, false);
  for (size_t k = 0; k < b.rects.size(); ++k) {
    const int i = b.first + int(k);
    const TileRect& r = b.rects[k];
    const ListItem& it = items[i];
    const int mid = r.r0 + kButtonRows / 2;
    const int inW = r.c1 - r.c0 - 1;  // inside the outline
    const int lw = int(it.label.size());
    int rw = int(it.right.size());
    if (rw > 0 && lw + 1 + rw > inW) {
      // Too long for both: keepRight splits the room, else `right` goes.
      rw = keepRight ? std::max(0, inW - 1 - std::min(lw, std::max(inW - 1 - rw, (inW - 1) / 2))) : 0;
    }
    if (rw > 0) g.putText(r.c1 - rw, mid, it.right, t.dim, t.bg, rw);
    const uint16_t fg = it.tone == ItemTone::Error ? t.error : it.marker ? t.selectBg : t.fg;
    g.putText(r.c0 + 1, mid, it.label, fg, t.bg, rw > 0 ? inW - rw - 1 : inW);
    g.outline(r.c0, r.r0, r.c1, r.r1, t.dim);
    if (activeTaps()) {
      Target tt;
      tt.kind = TargetKind::Row;
      tt.list = ListId::Main;
      tt.index = static_cast<int16_t>(i);
      activeTaps()->addCells(r.c0, r.r0, r.c1, r.r1, tt);
    }
  }
}

}  // namespace rc
