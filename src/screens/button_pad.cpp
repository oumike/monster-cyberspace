#include "screens/button_pad.h"

#include <algorithm>

#include "app/ui_task.h"
#include "hal/board.h"
#include "ui/button_grid.h"
#include "ui/tap_map.h"
#include "ui/widgets/scroll_bar.h"

namespace rc {

bool ButtonPad::active() { return board::hasTouch() && !board::hasKeys(); }

void ButtonPad::reset() {
  scroll_ = dragSteps_ = shift_ = selected_ = 0;
  hidden_ = ui::fromTouch();
}

int ButtonPad::columns() { return board::panelWidth() >= 800 ? 3 : 2; }

int ButtonPad::key(const KeyEvent& k) {
  if (cursor_ && ui::fromTouch()) {
    hidden_ = true;
    if (k.action == Action::Activate) {
      if (k.target != uint8_t(ListId::Main) || k.index < 0 || k.index >= count_) return -1;
      selected_ = k.index;
      return k.then == Action::Select ? k.index : -1;
    }
  } else if (cursor_ && hidden_) {
    switch (k.action) {
      case Action::Up:
      case Action::Down:
      case Action::Left:
      case Action::Right:
      case Action::Select:
        hidden_ = false;
        dragSteps_ = 0;
        selected_ = firstShown_;
        break;
      default:
        break;
    }
    return -1;
  } else if (cursor_) {
    switch (k.action) {
      case Action::Up:
        if (selected_ >= cols_) selected_ -= cols_;
        break;
      case Action::Down:
        if (selected_ + cols_ < count_) selected_ += cols_;
        else if (selected_ / cols_ < (count_ - 1) / cols_) selected_ = count_ - 1;
        break;
      case Action::Left:
        if (selected_ % cols_ > 0) --selected_;
        break;
      case Action::Right:
        if (selected_ % cols_ < cols_ - 1 && selected_ + 1 < count_) ++selected_;
        break;
      case Action::Select:
        return selected_ < count_ ? selected_ : -1;
      case Action::Activate:
        if (k.target != uint8_t(ListId::Main) || k.index < 0 || k.index >= count_) break;
        selected_ = k.index;
        if (k.then == Action::Select) return k.index;
        break;
      default:
        break;
    }
    return -1;
  }
  const int kPitch = height_ + 1;
  switch (k.action) {
    case Action::Up:
      if (scroll_ == 0 && shift_ > 0) {
        --shift_;
        dragSteps_ = 0;
        break;
      }
      if (--(dragSteps_ = std::min(dragSteps_, 0)) <= -kPitch) {
        dragSteps_ = 0;
        --scroll_;  // clamped in render
      }
      break;
    case Action::Down:
      if (shift_ < header_) {
        ++shift_;  // clamped in render
        dragSteps_ = 0;
        break;
      }
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
                       int bottom, bool keepRight, int cols, int header) {
  const int n = int(items.size());
  const bool details =
      std::any_of(items.begin(), items.end(), [](const ListItem& it) { return !it.detail.empty(); });
  height_ = kButtonRows + (details ? 1 : 0);
  cols = cols > 0 ? cols : columns();
  count_ = n;
  cols_ = cols;
  selected_ = std::clamp(selected_, 0, std::max(0, n - 1));
  // The header scrolls only as far as the buttons need, and fully away
  // before they scroll.
  header_ = std::max(0, header);
  int maxShift = 0;
  while (maxShift < header_ &&
         buttonGrid(g.cols(), top + header_ - maxShift, bottom, n, cols, 0, height_).maxScroll > 0)
    ++maxShift;
  header_ = maxShift;
  shift_ = std::min(std::max(0, shift_), maxShift);
  if (shift_ < maxShift) scroll_ = 0;
  auto layout = [&] {
    return buttonGrid(g.cols(), top + std::max(0, header) - shift_, bottom, n, cols, scroll_, height_);
  };
  const bool showCursor = cursor_ && !hidden_;
  // On the first row the header comes back too, as far as it fits.
  if (showCursor && selected_ < cols) scroll_ = shift_ = 0;
  ButtonGrid b = layout();
  // Keep the cursor shown: the header scrolls away first and comes back last.
  for (int guard = 0; showCursor && n > 0 && guard < n + header_ + 2; ++guard) {
    if (selected_ < b.first) {
      if (scroll_ > 0) --scroll_;
      else if (shift_ > 0) --shift_;
      else break;
    } else if (selected_ >= b.first + int(b.rects.size())) {
      if (shift_ < maxShift) ++shift_;
      else if (scroll_ < b.maxScroll) ++scroll_;
      else break;
    } else {
      break;
    }
    b = layout();
  }
  firstShown_ = b.first;
  scroll_ = b.scrollRow;
  if (activeTaps()) {
    activeTaps()->setList(ListId::Main, showCursor ? selected_ : -1, n, false);
    // Under the buttons, the whole area (gaps and header too) takes drags;
    // a tap there picks nothing.
    Target bg;
    bg.kind = TargetKind::Row;
    bg.list = ListId::Main;
    bg.index = -1;
    activeTaps()->addCells(0, top, g.cols() - 1, bottom - 1, bg);
  }
  for (size_t k = 0; k < b.rects.size(); ++k) {
    const int i = b.first + int(k);
    const TileRect& r = b.rects[k];
    const ListItem& it = items[i];
    const int mid = r.r0 + 1;  // the label row; a detail goes under it
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
    if (!it.detail.empty()) g.putText(r.c0 + 1, mid + 1, oneLine(it.detail, inW), t.dim, t.bg, inW);
    const bool sel = showCursor && i == selected_ && cursorShown();
    g.outline(r.c0, r.r0, r.c1, r.r1, sel ? t.selectBg : t.dim);
    // Outlined themes have one ink for both outlines: the cursor is a second
    // box, where it clears the label row.
    if (sel && t.outlined && r.r1 - r.r0 >= 3 && r.c1 - r.c0 >= 2)
      g.outline(r.c0 + 1, r.r0 + 1, r.c1 - 1, r.r1 - 1, t.selectBg);
    if (activeTaps()) {
      Target tt;
      tt.kind = TargetKind::Row;
      tt.list = ListId::Main;
      tt.index = static_cast<int16_t>(i);
      activeTaps()->addCells(r.c0, r.r0, r.c1, r.r1, tt);
    }
  }
  if (header > 0) {
    const int pitch = height_ + 1;
    drawScrollBar(g, g.cols() - 1, top, bottom, header + b.totalRows * pitch, bottom - top,
                  shift_ + b.scrollRow * pitch, t, t.bg);
  } else {
    drawScrollBar(g, g.cols() - 1, top, bottom, b.totalRows, b.visibleRows, b.scrollRow, t, t.bg);
  }
}

}  // namespace rc
