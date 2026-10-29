#include "input/touch_resolve.h"

#include <algorithm>
#include <cstdlib>

namespace rc {

void TouchResolver::reset() { carry_ = 0; }

// Moves list `list` by `steps` items. A finger moving towards later items
// (up on the glass, or Swipe Up) raises the index; earlier lowers it.
void TouchResolver::stepList(const TapMap& m, ListId list, int steps,
                             bool towardLater, TouchOutput& out) {
  if (steps <= 0) return;
  const ListInfo* li = m.list(list);
  if (!li) return;
  if (li->wraps) {
    int idx = li->selected + (towardLater ? steps : -steps);
    if (idx < 0) idx = 0;
    if (idx > li->count - 1) idx = li->count - 1;
    if (idx == li->selected) return;
    KeyEvent e;
    e.action = Action::Activate;
    e.index = (int16_t)idx;
    e.target = (uint8_t)list;
    e.then = Action::None;
    out.keys.push_back(e);
  } else {
    Action a = towardLater ? Action::Down : Action::Up;
    for (int i = 0; i < steps; ++i) {
      KeyEvent e;
      e.action = a;
      out.keys.push_back(e);
    }
  }
}

void TouchResolver::resolve(const Gesture& g, const TapMap& m, TouchOutput& out) {
  out.keys.clear();
  out.oskKey = -1;
  out.hideBanner = false;

  if (g.kind == GestureKind::EdgeBack || g.kind == GestureKind::EdgeOptions) {
    KeyEvent e;
    e.action = g.kind == GestureKind::EdgeBack ? Action::Back : Action::Options;
    out.keys.push_back(e);
    return;
  }

  Target t = m.hit(g.x, g.y);

  switch (g.kind) {
    case GestureKind::Hold:
      return;

    case GestureKind::Tap:
    case GestureKind::LongPress: {
      bool hold = g.kind == GestureKind::LongPress;
      switch (t.kind) {
        case TargetKind::Row: {
          const ListInfo* li = m.list(t.list);
          if (touchOnly_ && li && li->holdToAct) {
            // Taps scroll safely past; a long press acts on the row.
            if (!hold) break;
            KeyEvent e;
            e.action = Action::Activate;
            e.index = t.index;
            e.target = (uint8_t)t.list;
            e.then = Action::Select;
            out.keys.push_back(e);
            break;
          }
          KeyEvent e;
          e.action = Action::Activate;
          e.index = t.index;
          e.target = (uint8_t)t.list;
          e.then = hold ? Action::Palette : Action::Select;
          out.keys.push_back(e);
          break;
        }
        case TargetKind::Fixed: {
          if (hold && t.hold.action != Action::None) {
            out.keys.push_back(t.hold);
          } else {
            out.keys.push_back(t.tap);
          }
          break;
        }
        case TargetKind::Field: {
          KeyEvent e;
          e.action = Action::Focus;
          e.index = t.index;
          e.target = t.field;
          out.keys.push_back(e);
          break;
        }
        case TargetKind::Osk:
          out.oskKey = t.index;
          break;
        case TargetKind::Banner:
          if (!hold) out.hideBanner = true;
          break;
        default:
          break;
      }
      return;
    }

    case GestureKind::Drag: {
      if (t.kind != TargetKind::Row) {
        carry_ = 0;
        return;
      }
      carry_ += g.dy;
      int h = m.cell().h;
      if (h <= 0) return;
      int whole = carry_ / h;
      carry_ -= whole * h;
      if (whole == 0) return;
      bool towardLater = whole < 0;  // finger up (dy<0) → later items
      stepList(m, t.list, std::abs(whole), towardLater, out);
      return;
    }

    case GestureKind::Swipe: {
      if (t.kind != TargetKind::Row) return;
      if (g.dir != SwipeDir::Up && g.dir != SwipeDir::Down) return;
      int steps = std::max(1, m.visibleRows(t.list) - 1);
      bool towardLater = g.dir == SwipeDir::Up;  // finger up → later items
      stepList(m, t.list, steps, towardLater, out);
      return;
    }

    default:
      return;
  }
}

}  // namespace rc
