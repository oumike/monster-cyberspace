#include "ui/home_tiles.h"

#include <algorithm>
#include <cstdlib>

namespace rc {

HomeLayout homeLayout(int panelW, int areaPx, int gridRows, CellSize cell, int count,
                      int percent, bool gaps, bool spanFirst) {
  HomeLayout out;
  if (count <= 0 || cell.w <= 0 || cell.h <= 0 || areaPx <= 0) return out;
  // Across: an odd number of cells, so the gap column leaves equal halves.
  TileSpan across = tileSpan(panelW, cell.w, percent);
  if (across.cells % 2 == 0) --across.cells;
  const int tileW = (across.cells - 1) / 2;
  if (tileW < 10) return out;
  // Down: numRows tiles of h rows with gap px between them.
  const int numRows = spanFirst ? 1 + count / 2 : (count + 1) / 2;
  const int gap = gaps ? cell.w : 0;
  const int gapRows = gaps ? 1 : 0;
  const int avail = areaPx * percent / 100 - (numRows - 1) * gap;
  int h = avail > 0 ? (avail / numRows + cell.h / 2) / cell.h : 0;
  auto totalPx = [&](int rows) { return numRows * rows * cell.h + (numRows - 1) * gap; };
  auto lastRow = [&](int rows) { return 1 + numRows * rows + (numRows - 1) * gapRows; };
  while (h >= 3 && (totalPx(h) > areaPx || lastRow(h) > gridRows - 2)) --h;
  if (h < 3) return out;

  out.shiftPx = (panelW - across.cells * cell.w) / 2;
  out.lastRow = lastRow(h);
  out.pads.emplace_back(1, (areaPx - totalPx(h)) / 2);
  out.tiles.reserve(count);
  for (int i = 0; i < numRows; ++i) {
    const int r0 = 2 + i * (h + gapRows);
    if (gaps && i + 1 < numRows) out.pads.emplace_back(r0 + h, gap);
    if (spanFirst && i == 0) {
      out.tiles.push_back(TileRect{0, r0, across.cells - 1, r0 + h - 1});
      continue;
    }
    const int first = spanFirst ? i * 2 - 1 : i * 2;
    for (int j = 0; j < 2 && first + j < count; ++j) {
      const int c0 = j * (tileW + 1);
      out.tiles.push_back(TileRect{c0, r0, c0 + tileW - 1, r0 + h - 1});
    }
  }
  return out;
}

HomeLayout homeColumn(int panelW, int areaPx, int gridRows, CellSize cell, int count,
                      int percent, bool gaps) {
  HomeLayout out;
  if (count <= 0 || cell.w <= 0 || cell.h <= 0 || areaPx <= 0) return out;
  const int across = tileSpan(panelW, cell.w, percent).cells;
  if (across < 10) return out;
  constexpr int h = 3;
  const int gap = gaps ? cell.w : 0;
  const int gapRows = gaps ? 1 : 0;
  auto totalPx = [&](int n) { return n * h * cell.h + (n - 1) * gap; };
  auto lastRow = [&](int n) { return 1 + n * h + (n - 1) * gapRows; };
  int n = 0;
  while (n < count && totalPx(n + 1) <= areaPx && lastRow(n + 1) <= gridRows - 2) ++n;
  if (n == 0) return out;

  out.shiftPx = (panelW - across * cell.w) / 2;
  out.lastRow = lastRow(n);
  out.pads.emplace_back(1, (areaPx - totalPx(n)) / 2);
  out.tiles.reserve(n);
  for (int i = 0; i < n; ++i) {
    const int r0 = 2 + i * (h + gapRows);
    if (gaps && i + 1 < n) out.pads.emplace_back(r0 + h, gap);
    out.tiles.push_back(TileRect{0, r0, across - 1, r0 + h - 1});
  }
  return out;
}

HomeButtons homeButtons(int panelW, int areaPx, CellSize cell, int count, int labelCells, int padY,
                        bool spanFirst) {
  HomeButtons out;
  if (count <= 0 || cell.w <= 0 || cell.h <= 0 || labelCells <= 0) return out;
  const int inset = kHomePadPx + 1;
  const int w = labelCells * cell.w + 2 * inset;
  const int h = cell.h + 2 * (padY + 1);
  if (2 * w + kHomeGapPx > panelW) return out;
  spanFirst = spanFirst && count > 1;
  // Item i's row and side; a first item alone across the top row leaves the
  // rest to pair up under it, odd ones on the right.
  auto place = [&](int i, int& row, int8_t& side) {
    if (spanFirst && i == 0) {
      row = 0;
      side = 0;
    } else if (spanFirst) {
      row = 1 + (i - 1) / 2;
      side = i % 2 == 0 ? -1 : 1;
    } else {
      row = i / 2;
      side = i % 2 == 0 ? -1 : 1;
    }
  };
  int rows = 0;
  for (int i = 0; i < count; ++i) {
    int row;
    int8_t side;
    place(i, row, side);
    rows = std::max(rows, row + 1);
  }
  if (rows * (h + kHomeGapPx) > areaPx) return out;
  out.rects.reserve(count);
  out.side.reserve(count);
  for (int i = 0; i < count; ++i) {
    int row;
    int8_t side;
    place(i, row, side);
    const int x = side > 0 ? panelW - w : 0;
    out.rects.push_back(PxRect{x, kHomeGapPx + row * (h + kHomeGapPx), side == 0 ? panelW : w, h});
    out.side.push_back(side);
  }
  return out;
}

void widenHomeButtons(HomeButtons& b, int panelW) {
  const int w = (panelW - kHomeGapPx) / 2;
  for (size_t i = 0; i < b.rects.size() && i < b.side.size(); ++i) {
    PxRect& r = b.rects[i];
    if (b.side[i] == 0 || r.w >= w) continue;
    r.w = w;
    r.x = b.side[i] > 0 ? panelW - w : 0;
  }
}

int buttonMove(const std::vector<PxRect>& rects, int selected, Action a) {
  if (selected < 0 || selected >= int(rects.size())) return selected;
  const PxRect& s = rects[selected];
  const int sx = s.x + s.w / 2, sy = s.y + s.h / 2;
  int best = selected;
  long bestD = 0;
  for (int i = 0; i < int(rects.size()); ++i) {
    if (i == selected) continue;
    const PxRect& r = rects[i];
    const int dx = r.x + r.w / 2 - sx, dy = r.y + r.h / 2 - sy;
    const bool sameRow = r.y < s.y + s.h && s.y < r.y + r.h;
    long d;
    switch (a) {
      case Action::Up:
        if (dy >= 0 || sameRow) continue;
        d = long(-dy) * 4096 + std::abs(dx);
        break;
      case Action::Down:
        if (dy <= 0 || sameRow) continue;
        d = long(dy) * 4096 + std::abs(dx);
        break;
      case Action::Left:
        if (dx >= 0 || !sameRow) continue;
        d = -dx;
        break;
      case Action::Right:
        if (dx <= 0 || !sameRow) continue;
        d = dx;
        break;
      default:
        return selected;
    }
    if (best == selected || d < bestD) {
      best = i;
      bestD = d;
    }
  }
  return best;
}

TileSpan tileSpan(int areaPx, int unitPx, int percent) {
  if (areaPx <= 0 || unitPx <= 0) return TileSpan{0, 0};
  int cells = (areaPx * percent / 100 + unitPx / 2) / unitPx;
  cells = std::min(cells, areaPx / unitPx);
  return TileSpan{cells, (areaPx - cells * unitPx) / 2};
}

int tileMove(int selected, int count, Action a, bool spanFirst) {
  if (count <= 0) return selected;
  if (spanFirst) {
    // The rest are a plain two-column grid of count - 1 tiles.
    if (selected == 0) return a == Action::Down && count > 1 ? 1 : 0;
    if (a == Action::Up && selected <= 2) return 0;
    return 1 + tileMove(selected - 1, count - 1, a);
  }
  const int col = selected % 2;
  const int row = selected / 2;
  switch (a) {
    case Action::Up:
      if (row > 0) selected -= 2;
      break;
    case Action::Down:
      if (selected + 2 < count) selected += 2;
      break;
    case Action::Left:
      if (col > 0) selected -= 1;
      break;
    case Action::Right:
      if (col == 0 && selected + 1 < count) selected += 1;
      break;
    default:
      break;
  }
  return selected;
}

}  // namespace rc
