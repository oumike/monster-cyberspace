#include "ui/tap_map.h"

namespace rc {

namespace {
TapMap* g_active = nullptr;
}

TapMap* activeTaps() { return g_active; }
void setActiveTaps(TapMap* m) { g_active = m; }

void TapMap::clear() {
  rects_.clear();
  rects_.reserve(128);
  lists_.clear();
}

void TapMap::setOrigin(CellSize cell, int firstRow) {
  cell_ = cell;
  firstRow_ = firstRow;
}

void TapMap::addCells(int c0, int r0, int c1, int r1, const Target& t) {
  Rect rc;
  rc.x0 = c0 * cell_.w;
  rc.x1 = (c1 + 1) * cell_.w - 1;
  rc.y0 = (firstRow_ + r0) * cell_.h;
  rc.y1 = (firstRow_ + r1 + 1) * cell_.h - 1;
  rc.t = t;
  rects_.push_back(rc);
}

void TapMap::setList(ListId id, int selected, int count, bool wraps) {
  for (auto& li : lists_) {
    if (li.list == id) {
      li.selected = selected;
      li.count = count;
      li.wraps = wraps;
      return;
    }
  }
  lists_.push_back(ListInfo{id, selected, count, wraps});
}

const ListInfo* TapMap::list(ListId id) const {
  for (const auto& li : lists_) {
    if (li.list == id) return &li;
  }
  return nullptr;
}

int TapMap::visibleRows(ListId id) const {
  int n = 0;
  for (const auto& r : rects_) {
    if (r.t.kind == TargetKind::Row && r.t.list == id) ++n;
  }
  return n;
}

Target TapMap::hit(int x, int y) const {
  // Last registered wins (topmost).
  for (auto it = rects_.rbegin(); it != rects_.rend(); ++it) {
    if (x < it->x0 || x > it->x1 || y < it->y0 || y > it->y1) continue;
    Target t = it->t;
    if (t.kind == TargetKind::Field && cell_.w > 0) {
      int col = (x - it->x0) / cell_.w;
      int pos = t.first + col;
      int cap = t.first + t.len;
      if (pos > cap) pos = cap;
      t.index = (int16_t)pos;
    }
    return t;
  }
  return Target{};
}

}  // namespace rc
