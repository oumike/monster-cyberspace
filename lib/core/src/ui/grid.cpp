#include "ui/grid.h"

#include <algorithm>

#include "ui/text_width.h"

namespace rc {

Grid::Grid(int cols, int rows) { resize(cols, rows); }

void Grid::resize(int cols, int rows) {
  cols_ = std::max(cols, 0);
  rows_ = std::max(rows, 0);
  cells_.assign(static_cast<size_t>(cols_) * rows_, Cell{});
  drawn_ = cells_;
  forced_.assign(rows_, true);
  kinds_.assign(rows_, RowKind::Normal);
  pads_.assign(rows_, -1);
  shifts_.assign(rows_, 0);
}

void Grid::fill(uint16_t bg) {
  for (int r = 0; r < rows_; ++r) fillRow(r, bg);
  std::fill(kinds_.begin(), kinds_.end(), RowKind::Normal);
  std::fill(pads_.begin(), pads_.end(), int16_t(-1));
  std::fill(shifts_.begin(), shifts_.end(), int16_t(0));
}

void Grid::setRowKind(int row, RowKind k) {
  if (row >= 0 && row < rows_) kinds_[row] = k;
}

RowKind Grid::rowKind(int row) const {
  return row >= 0 && row < rows_ ? kinds_[row] : RowKind::Normal;
}

void Grid::setRowPad(int row, int px) {
  if (row >= 0 && row < rows_) pads_[row] = static_cast<int16_t>(px < 0 ? -1 : px);
}

int Grid::rowPad(int row) const { return row >= 0 && row < rows_ ? pads_[row] : -1; }

void Grid::setRowShift(int row, int px) {
  if (row >= 0 && row < rows_) shifts_[row] = static_cast<int16_t>(std::max(px, 0));
}

int Grid::rowShift(int row) const { return row >= 0 && row < rows_ ? shifts_[row] : 0; }

void Grid::addEdges(int col, int row, uint8_t edges, uint16_t color) {
  if (col < 0 || row < 0 || col >= cols_ || row >= rows_) return;
  Cell& cell = cells_[static_cast<size_t>(row) * cols_ + col];
  cell.edges |= edges;
  cell.edgeColor = color;
}

void Grid::fillRow(int row, uint16_t bg) {
  for (int c = 0; c < cols_; ++c) put(c, row, U' ', 0, bg);
}

void Grid::put(int col, int row, char32_t cp, uint16_t fg, uint16_t bg) {
  if (col < 0 || row < 0 || col >= cols_ || row >= rows_) return;
  cells_[static_cast<size_t>(row) * cols_ + col] = Cell{cp, fg, bg};
}

void Grid::outline(int c0, int r0, int c1, int r1, uint16_t color) {
  c0 = std::max(c0, 0);
  r0 = std::max(r0, 0);
  c1 = std::min(c1, cols_ - 1);
  r1 = std::min(r1, rows_ - 1);
  for (int r = r0; r <= r1; ++r) {
    for (int c = c0; c <= c1; ++c) {
      uint8_t e = (r == r0 ? kEdgeTop : 0) | (r == r1 ? kEdgeBottom : 0) |
                  (c == c0 ? kEdgeLeft : 0) | (c == c1 ? kEdgeRight : 0);
      if (!e) continue;
      Cell& cell = cells_[static_cast<size_t>(r) * cols_ + c];
      cell.edges |= e;
      cell.edgeColor = color;
    }
  }
}

int Grid::putText(int col, int row, std::u32string_view s, uint16_t fg, uint16_t bg,
                  int maxCols) {
  int n = 0;
  for (char32_t cp : s) {
    const int w = cellWidth(cp);
    if (w == 0) continue;
    if (n + w > maxCols || col + n + w > cols_) break;
    put(col + n, row, cp, fg, bg);
    if (w == 2) put(col + n + 1, row, wideTail(cp), fg, bg);
    n += w;
  }
  return n;
}

const Cell& Grid::at(int col, int row) const {
  static const Cell blank{};
  if (col < 0 || row < 0 || col >= cols_ || row >= rows_) return blank;
  return cells_[static_cast<size_t>(row) * cols_ + col];
}

bool Grid::rowDirty(int row) const {
  if (row < 0 || row >= rows_) return false;
  if (forced_[row]) return true;
  auto first = cells_.begin() + static_cast<size_t>(row) * cols_;
  return !std::equal(first, first + cols_, drawn_.begin() + static_cast<size_t>(row) * cols_);
}

void Grid::clearDirty() {
  drawn_ = cells_;
  std::fill(forced_.begin(), forced_.end(), false);
}

TextSize nextTextSize(TextSize s) {
  switch (s) {
    case TextSize::Compact: return TextSize::Comfortable;
    case TextSize::Comfortable: return TextSize::Large;
    default: return TextSize::Compact;
  }
}

const char* textSizeKey(TextSize s) {
  switch (s) {
    case TextSize::Compact: return "compact";
    case TextSize::Large: return "large";
    default: return "comfortable";
  }
}

bool parseTextSize(std::string_view key, TextSize& out) {
  for (TextSize s : {TextSize::Compact, TextSize::Comfortable, TextSize::Large}) {
    if (key == textSizeKey(s)) {
      out = s;
      return true;
    }
  }
  return false;
}

std::u32string textSizeLabel(TextSize s) {
  switch (s) {
    case TextSize::Compact: return U"Compact";
    case TextSize::Large: return U"Large";
    default: return U"Comfortable";
  }
}

TextSize textSizeFromIndex(int i) {
  return i >= 0 && i <= static_cast<int>(TextSize::Large) ? static_cast<TextSize>(i) : TextSize::Compact;
}

CellSize cellSize(TextSize s) {
  switch (s) {
    case TextSize::Compact: return CellSize{6, 12};
    case TextSize::Large: return CellSize{10, 18};
    default: return CellSize{8, 16};
  }
}

void gridDims(int panelW, int panelH, TextSize s, int& cols, int& rows, int headerPx) {
  CellSize c = cellSize(s);
  cols = panelW / c.w;
  rows = headerPx > 0 ? std::max(0, panelH - headerPx) / c.h + 1 : panelH / c.h;
}

CellSize bigCellSize(TextSize s) {
  switch (s) {
    case TextSize::Compact: return CellSize{8, 14};
    case TextSize::Large: return CellSize{12, 24};
    default: return CellSize{10, 18};
  }
}

int bigCols(int panelW, TextSize s) { return panelW / bigCellSize(s).w; }

HeaderSize touchHeader(int panelW, TextSize s) {
  HeaderSize h;
  h.px = (cellSize(s).h * 3 + 1) / 2;
  h.cellW = bigCellSize(s).w;
  h.cols = bigCols(panelW, s);
  return h;
}

std::vector<RowSpan> rowSpans(const Grid& g, int rowH, int bigH) {
  const int n = g.rows();
  std::vector<RowSpan> out(n, RowSpan{0, 0});
  if (n == 0) return out;
  const int headPx = g.header().px;
  const int lastY = (n - 1) * rowH + (headPx > 0 && n > 1 ? std::max(0, headPx - rowH) : 0);
  int y = 0;
  for (int r = 0; r < n - 1; ++r) {
    RowKind k = g.rowKind(r);
    int h = k == RowKind::Skip ? 0 : k == RowKind::Big ? bigH : rowH;
    if (g.rowPad(r) >= 0) h = g.rowPad(r);
    if (r == 0 && headPx > 0) h = headPx;
    h = std::max(0, std::min(h, lastY - y));
    out[r] = RowSpan{y, h, g.rowShift(r)};
    y += h;
  }
  out[n - 1] = RowSpan{lastY, rowH + g.bottomPad(), g.rowShift(n - 1)};
  return out;
}

}  // namespace rc
