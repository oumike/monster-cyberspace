#include "ui/renderer.h"

#include <esp_heap_caps.h>

#include <algorithm>

#include "hal/board.h"
#include "ui/fonts/cell10x18.h"
#include "ui/fonts/cell12x24.h"
#include "ui/fonts/cell6x12.h"
#include "ui/fonts/cell8x14.h"
#include "ui/fonts/cell8x16.h"
#include "ui/fonts/emoji_cells.h"
#include "ui/widgets/scroll_bar.h"

namespace rc {

// A whole frame in PSRAM, or strip mode: a strip one pixel line tall.
bool Renderer::allocate() {
  heap_caps_free(frame_);
  stripRows_ = 0;
  base_ = 0;
  frame_ = static_cast<uint16_t*>(
      heap_caps_calloc(static_cast<size_t>(w_) * h_, sizeof(uint16_t), MALLOC_CAP_SPIRAM));
  strip_ = frame_ == nullptr;
  return frame_ || ensureStrip(1);
}

bool Renderer::ensureStrip(int rows) {
  if (rows <= stripRows_) return true;
  heap_caps_free(frame_);
  frame_ = static_cast<uint16_t*>(heap_caps_malloc(static_cast<size_t>(w_) * rows * sizeof(uint16_t),
                                                   MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  stripRows_ = frame_ ? rows : 0;
  return frame_ != nullptr;
}

// pushRows takes a whole frame, so the strip goes as one shifted back by
// base_ rows; the board reads only rows [y0, y1).
void Renderer::push(int y0, int y1) {
  if (!strip_) {
    board::pushRows(frame_, y0, y1);
    return;
  }
  const uintptr_t shifted =
      reinterpret_cast<uintptr_t>(frame_) - static_cast<uintptr_t>(base_) * w_ * sizeof(uint16_t);
  board::pushRows(reinterpret_cast<const uint16_t*>(shifted), y0, y1);
}

bool Renderer::begin() {
  w_ = board::panelWidth();
  h_ = board::panelHeight();
  return allocate();
}

void Renderer::resize() {
  const int w = board::panelWidth(), h = board::panelHeight();
  const bool grew = static_cast<size_t>(w) * h > static_cast<size_t>(w_) * h_;
  w_ = w;
  h_ = h;
  if (strip_) {
    heap_caps_free(frame_);
    frame_ = nullptr;
    stripRows_ = 0;
    ensureStrip(1);
  } else if (grew) {
    allocate();
  }
  main_ = Layer{0};
  docked_ = Layer{};
}

void Renderer::drawRow(const Grid& g, int r, const CellFont& font, RowSpan span, int yoff) {
  yoff = std::clamp(yoff, 0, span.h);
  const int cw = font.w, ch = std::min<int>(font.h, span.h - yoff);
  const int ox = std::clamp(span.x, 0, w_);
  const int rb = rowBytes(font);
  const int cols = std::min(g.cols(), (w_ - ox) / cw);
  const int used = ox + cols * cw;
  const uint16_t bg0 = g.cols() > 0 ? g.at(0, r).bg : 0;
  int barX = -1;  // a scroll bar's left px in this row
  uint16_t barColor = 0;
  const int barW = std::max(2, cw / 4);
  // Each cell's glyph, and the glyph a nudged cell before it spills into it.
  glyphs_.assign(static_cast<size_t>(cols), nullptr);
  for (int c = 0; c < cols; ++c) {
    const Cell& cell = g.at(c, r);
    if (cell.cp == kScrollBarCp) {
      // The last cell's bar goes on the panel edge, as its right edge does.
      barX = (c == cols - 1 ? w_ : ox + (c + 1) * cw) - barW;
      barColor = cell.fg;
    } else {
      glyphs_[c] = cellGlyph(font, cell.cp);
    }
  }
  const int half = cw / 2;
  auto rowBits = [&](const uint8_t* gl, int y) -> uint16_t {
    return rb == 1 ? gl[y] << 8 : (gl[y * 2] << 8) | gl[y * 2 + 1];
  };
  // A pixel line at a time, so strip mode needs a strip one line tall.
  for (int y = span.y; y < span.y + span.h && y < h_; ++y) {
    if (strip_) base_ = y;
    uint16_t* px = row(y);
    const int gy = y - span.y - yoff;  // the glyph line, outside [0, ch) padding
    if (gy < 0 || gy >= ch) {
      if (g.cols() > 0)
        for (int x = 0; x < w_; ++x) px[x] = bg0;
    } else {
      // A shifted row: the strip on the left takes cell 0's background.
      for (int x = 0; x < ox; ++x) px[x] = bg0;
      for (int c = 0; c < cols; ++c) {
        const Cell& cell = g.at(c, r);
        uint16_t* cp = px + ox + c * cw;
        // A nudged cell's glyph sits half a cell right, its right part in the
        // next cell.
        const Cell* prev = c > 0 ? &g.at(c - 1, r) : nullptr;
        const uint8_t* spill = prev && prev->nudge ? glyphs_[c - 1] : nullptr;
        uint16_t bits = glyphs_[c] ? rowBits(glyphs_[c], gy) : 0;
        if (cell.bold) bits |= bits >> 1;
        if (cell.nudge) bits >>= half;
        if (spill) {
          uint16_t sb = rowBits(spill, gy);
          if (prev->bold) sb |= sb >> 1;
          bits |= uint16_t(sb << (cw - half));
        }
        for (int x = 0; x < cw; ++x) cp[x] = (bits & (0x8000 >> x)) ? cell.fg : cell.bg;
        if (cell.edges) {
          const uint16_t ec = cell.edgeColor;
          if ((gy == 0 && (cell.edges & kEdgeTop)) || (gy == ch - 1 && (cell.edges & kEdgeBottom)))
            for (int x = 0; x < cw; ++x) cp[x] = ec;
          if (cell.edges & kEdgeLeft) cp[0] = ec;
          // The last cell's right side goes on the panel edge (below), so rows
          // of different widths line up.
          if ((cell.edges & kEdgeRight) && !(c == cols - 1 && used < w_)) cp[cw - 1] = ec;
        }
      }
      // Pixels right of the last whole cell take the last cell's background.
      if (used < w_ && cols > 0) {
        const Cell& last = g.at(cols - 1, r);
        const uint16_t ec = last.edgeColor;
        const bool line =
            (gy == 0 && (last.edges & kEdgeTop)) || (gy == ch - 1 && (last.edges & kEdgeBottom));
        for (int x = used; x < w_; ++x) px[x] = line ? ec : last.bg;
        if (last.edges & kEdgeRight) px[w_ - 1] = ec;
      }
    }
    // Over the cell's blank and any padding, the whole row tall, so a bar down
    // several rows is unbroken.
    if (barX >= 0)
      for (int x = std::max(barX, 0); x < barX + barW && x < w_; ++x) px[x] = barColor;
    if (strip_) push(y, y + 1);
  }
  if (!strip_) push(span.y, span.y + span.h);
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
      // In strip mode a strip at a time.
      for (int y0 = gapY; y0 < gapEnd;) {
        const int y1 = strip_ ? std::min(gapEnd, y0 + stripRows_) : gapEnd;
        if (strip_) base_ = y0;
        for (int y = y0; y < y1; ++y) {
          uint16_t* px = row(y);
          for (int x = 0; x < w_; ++x) px[x] = bg;
        }
        push(y0, y1);
        y0 = y1;
      }
    }
    layer.gapY = gapY;
  }
  layer.spans = std::move(spans);
  g.clearDirty();
}

}  // namespace rc
