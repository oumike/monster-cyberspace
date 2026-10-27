#include "ui/renderer.h"

#include <esp_heap_caps.h>

#include <algorithm>

#include "hal/board.h"
#include "ui/fonts/cell10x18.h"
#include "ui/fonts/cell12x24.h"
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

void Renderer::resize() {
  const int w = board::panelWidth(), h = board::panelHeight();
  if (static_cast<size_t>(w) * h > static_cast<size_t>(w_) * h_) {
    heap_caps_free(frame_);
    frame_ = static_cast<uint16_t*>(
        heap_caps_calloc(static_cast<size_t>(w) * h, sizeof(uint16_t), MALLOC_CAP_SPIRAM));
  }
  w_ = w;
  h_ = h;
  main_ = Layer{0};
  docked_ = Layer{};
}

void Renderer::drawRow(const Grid& g, int r, const CellFont& font, RowSpan span, int yoff) {
  yoff = std::clamp(yoff, 0, span.h);
  const int cw = font.w, ch = std::min<int>(font.h, span.h - yoff), y0 = span.y + yoff;
  if (yoff > 0 && g.cols() > 0) {
    const uint16_t bg = g.at(0, r).bg;
    for (int y = span.y; y < y0; ++y) {
      uint16_t* px = frame_ + static_cast<size_t>(y) * w_;
      for (int x = 0; x < w_; ++x) px[x] = bg;
    }
  }
  const int ox = std::clamp(span.x, 0, w_);
  const int rb = rowBytes(font);
  const int cols = std::min(g.cols(), (w_ - ox) / cw);
  const int used = ox + cols * cw;
  // A shifted row: the strip on the left takes cell 0's background.
  if (ox > 0 && g.cols() > 0) {
    const uint16_t bg = g.at(0, r).bg;
    for (int y = 0; y < ch; ++y) {
      uint16_t* px = frame_ + static_cast<size_t>(y0 + y) * w_;
      for (int x = 0; x < ox; ++x) px[x] = bg;
    }
  }
  for (int c = 0; c < cols; ++c) {
    const Cell& cell = g.at(c, r);
    const int x0 = ox + c * cw;
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
  // A pad row taller than the font: the rest takes the row's background.
  if (span.h - yoff > ch && g.cols() > 0) {
    const uint16_t bg = g.at(0, r).bg;
    for (int y = ch; y < span.h - yoff; ++y) {
      uint16_t* px = frame_ + static_cast<size_t>(y0 + y) * w_;
      for (int x = 0; x < w_; ++x) px[x] = bg;
    }
  }
  board::pushRows(frame_, span.y, span.y + span.h);
}

void Renderer::draw(Grid& g, TextSize s, int firstRow, int offsetPx) {
  if (!frame_) return;
  const CellFont& font = s == TextSize::Compact ? cell6x12 : s == TextSize::Large ? cell10x18 : cell8x16;
  const CellFont& big = s == TextSize::Compact ? cell8x14 : s == TextSize::Large ? cell12x24 : cell10x18;
  Layer& layer = firstRow == 0 ? main_ : docked_;
  const int top = firstRow * font.h + offsetPx;
  if (layer.firstRow != firstRow || layer.top != top) layer = Layer{firstRow, top};
  const int n = g.rows();
  std::vector<RowSpan> spans = rowSpans(g, font.h, big.h);
  const bool sameShape = layer.spans.size() == spans.size();
  bool aboveGapChanged = false;
  for (int r = 0; r < n; ++r) {
    const RowSpan sp = spans[r];
    const bool moved = !sameShape || layer.spans[r].y != sp.y || layer.spans[r].h != sp.h ||
                       layer.spans[r].x != sp.x;
    if (!moved && !g.rowDirty(r)) continue;
    if (r == n - 2) aboveGapChanged = true;
    if (sp.h <= 0 || top + sp.y + sp.h > h_) continue;
    const bool head = r == 0 && g.header().px > 0;
    drawRow(g, r, head || g.rowKind(r) == RowKind::Big ? big : font,
            RowSpan{top + sp.y, sp.h, sp.x}, head ? (sp.h - big.h) / 2 : 0);
  }
  // Rows of other heights can leave a gap above the last row; clear it.
  if (n >= 2) {
    const int gapY = top + spans[n - 2].y + spans[n - 2].h;
    const int gapEnd = std::min(top + spans[n - 1].y, h_);
    if (gapY < gapEnd && (gapY != layer.gapY || aboveGapChanged || !sameShape)) {
      const uint16_t bg = g.at(0, n - 2).bg;
      for (int y = gapY; y < gapEnd; ++y) {
        uint16_t* px = frame_ + static_cast<size_t>(y) * w_;
        for (int x = 0; x < w_; ++x) px[x] = bg;
      }
      board::pushRows(frame_, gapY, gapEnd);
    }
    layer.gapY = gapY;
  }
  layer.spans = std::move(spans);
  g.clearDirty();
}

}  // namespace rc
