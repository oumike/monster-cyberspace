#include "screens/button_pad.h"

#include <algorithm>

#include "app/ui_task.h"
#include "hal/board.h"
#include "ui/button_grid.h"
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
        --scroll_;  // clamped in sync
      }
      break;
    case Action::Down:
      if (shift_ < header_) {
        ++shift_;  // clamped in sync
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

ButtonGrid ButtonPad::layout(int gridCols, const std::vector<ListItem>& items, int top, int bottom,
                             int cols, int header) {
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
         buttonGrid(gridCols, top + header_ - maxShift, bottom, n, cols, 0, height_).maxScroll > 0)
    ++maxShift;
  header_ = maxShift;
  shift_ = std::min(std::max(0, shift_), maxShift);
  if (shift_ < maxShift) scroll_ = 0;
  auto grid = [&] {
    return buttonGrid(gridCols, top + std::max(0, header) - shift_, bottom, n, cols, scroll_, height_);
  };
  const bool showCursor = cursor_ && !hidden_;
  // On the first row the header comes back too, as far as it fits.
  if (showCursor && selected_ < cols) scroll_ = shift_ = 0;
  ButtonGrid b = grid();
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
    b = grid();
  }
  firstShown_ = b.first;
  scroll_ = b.scrollRow;
  return b;
}

void ButtonPad::sync(lvui::TileGrid& v, const std::vector<ListItem>& items, int top, int bottom,
                     bool keepRight, int cols, int header) {
  lv_obj_t* parent = lv_obj_get_parent(v.obj());
  lv_obj_update_layout(parent);
  const lvui::Cell c = lvui::cellOf(v.obj());
  if (c.w <= 0 || c.h <= 0) return;
  const ButtonGrid b = layout(lv_obj_get_width(parent) / c.w, items, top, bottom, cols, header);
  lvui::TileGrid::Scroll scroll;
  if (header > 0) {
    const int pitch = height_ + 1;
    scroll = {header + b.totalRows * pitch, bottom - top, shift_ + b.scrollRow * pitch};
  } else {
    scroll = {b.totalRows, b.visibleRows, b.scrollRow};
  }
  v.syncButtons(b, items, cursor_ && !hidden_ ? selected_ : -1, keepRight, top, bottom, top * c.h, scroll);
}

}  // namespace rc
