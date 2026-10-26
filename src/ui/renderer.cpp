#include "ui/renderer.h"

#include <esp_heap_caps.h>

#include <algorithm>

#include "hal/board.h"
#include "ui/fonts/cell10x18.h"
#include "ui/fonts/cell6x12.h"
#include "ui/fonts/cell8x14.h"
#include "ui/fonts/cell8x16.h"

namespace rc {

bool Renderer::begin() {
  w_ = board::panelWidth();
  h_ = board::panelHeight();
  frame_ = static_cast<uint16_t*>(
      heap_caps_calloc(static_cast<size_t>(w_) * h_, sizeof(uint16_t), MALLOC_CAP_SPIRAM));
  return frame_ != nullptr;
}

void Renderer::drawRow(const Grid& g, int r, const CellFont& font, RowSpan span) {
  const int cw = font.w, ch = std::min<int>(font.h, span.h), y0 = span.y;
  const int rb = rowBytes(font);
  const int cols = std::min(g.cols(), w_ / cw);
  const int used = cols * cw;
  for (int c = 0; c < cols; ++c) {
    const Cell& cell = g.at(c, r);
    const int x0 = c * cw;
    const uint8_t* glyph = glyphFor(font, cell.cp);
    for (int y = 0; y < ch; ++y) {
      uint16_t bits = 0;
      if (glyph) bits = rb == 1 ? glyph[y] << 8 : (glyph[y * 2] << 8) | glyph[y * 2 + 1];
      uint16_t* px = frame_ + static_cast<size_t>(y0 + y) * w_ + x0;
      for (int x = 0; x < cw; ++x) px[x] = (bits & (0x8000 >> x)) ? cell.fg : cell.bg;
    }
    if (cell.edges) {
      const uint16_t ec = cell.edgeColor;
      uint16_t* top = frame_ + static_cast<size_t>(y0) * w_ + x0;
      uint16_t* bottom = top + static_cast<size_t>(ch - 1) * w_;
      if (cell.edges & kEdgeTop)
        for (int x = 0; x < cw; ++x) top[x] = ec;
      if (cell.edges & kEdgeBottom)
        for (int x = 0; x < cw; ++x) bottom[x] = ec;
      if (cell.edges & kEdgeLeft)
        for (int y = 0; y < ch; ++y) top[static_cast<size_t>(y) * w_] = ec;
      // The last cell's right side goes on the panel edge (below), so rows of
      // different widths line up.
      if ((cell.edges & kEdgeRight) && !(c == cols - 1 && used < w_))
        for (int y = 0; y < ch; ++y) top[static_cast<size_t>(y) * w_ + cw - 1] = ec;
    }
  }
  // Pixels right of the last whole cell take the last cell's background.
  if (used < w_ && cols > 0) {
    const Cell& last = g.at(cols - 1, r);
    for (int y = 0; y < ch; ++y) {
      uint16_t* px = frame_ + static_cast<size_t>(y0 + y) * w_;
      for (int x = used; x < w_; ++x) px[x] = last.bg;
    }
    const uint16_t ec = last.edgeColor;
    uint16_t* top = frame_ + static_cast<size_t>(y0) * w_;
    if (last.edges & kEdgeTop)
      for (int x = used; x < w_; ++x) top[x] = ec;
    if (last.edges & kEdgeBottom)
      for (int x = used; x < w_; ++x) top[static_cast<size_t>(ch - 1) * w_ + x] = ec;
    if (last.edges & kEdgeRight)
      for (int y = 0; y < ch; ++y) top[static_cast<size_t>(y) * w_ + w_ - 1] = ec;
  }
  board::pushRows(frame_, y0, y0 + ch);
}

void Renderer::draw(Grid& g, TextSize s) {
  if (!frame_) return;
  const CellFont& font = s == TextSize::Compact ? cell6x12 : cell8x16;
  const CellFont& big = s == TextSize::Compact ? cell8x14 : cell10x18;
  const int n = g.rows();
  std::vector<RowSpan> spans = rowSpans(g, font.h, big.h);
  const bool sameShape = spans_.size() == spans.size();
  bool aboveGapChanged = false;
  for (int r = 0; r < n; ++r) {
    const RowSpan sp = spans[r];
    const bool moved = !sameShape || spans_[r].y != sp.y || spans_[r].h != sp.h;
    if (!moved && !g.rowDirty(r)) continue;
    if (r == n - 2) aboveGapChanged = true;
    if (sp.h <= 0 || sp.y + sp.h > h_) continue;
    drawRow(g, r, g.rowKind(r) == RowKind::Big ? big : font, sp);
  }
  // Rows of other heights can leave a gap above the last row; clear it.
  if (n >= 2) {
    const int gapY = spans[n - 2].y + spans[n - 2].h;
    const int gapEnd = std::min(spans[n - 1].y, h_);
    if (gapY < gapEnd && (gapY != gapY_ || aboveGapChanged || !sameShape)) {
      const uint16_t bg = g.at(0, n - 2).bg;
      for (int y = gapY; y < gapEnd; ++y) {
        uint16_t* px = frame_ + static_cast<size_t>(y) * w_;
        for (int x = 0; x < w_; ++x) px[x] = bg;
      }
      board::pushRows(frame_, gapY, gapEnd);
    }
    gapY_ = gapY;
  }
  spans_ = std::move(spans);
  g.clearDirty();
}

}  // namespace rc
