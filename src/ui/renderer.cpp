#include "ui/renderer.h"

#include <esp_heap_caps.h>

#include "hal/board.h"
#include "ui/fonts/cell6x12.h"
#include "ui/fonts/cell8x16.h"

namespace rc {

bool Renderer::begin() {
  w_ = board::panelWidth();
  h_ = board::panelHeight();
  frame_ = static_cast<uint16_t*>(
      heap_caps_calloc(static_cast<size_t>(w_) * h_, sizeof(uint16_t), MALLOC_CAP_SPIRAM));
  return frame_ != nullptr;
}

void Renderer::draw(Grid& g, TextSize s) {
  if (!frame_) return;
  const CellFont& font = s == TextSize::Compact ? cell6x12 : cell8x16;
  const int cw = font.w, ch = font.h;
  for (int r = 0; r < g.rows(); ++r) {
    if (!g.rowDirty(r)) continue;
    const int y0 = r * ch;
    if (y0 + ch > h_) break;
    for (int c = 0; c < g.cols(); ++c) {
      const Cell& cell = g.at(c, r);
      const int x0 = c * cw;
      if (x0 + cw > w_) break;
      const uint8_t* glyph = glyphFor(font, cell.cp);
      for (int y = 0; y < ch; ++y) {
        uint8_t bits = glyph ? glyph[y] : 0;
        uint16_t* px = frame_ + static_cast<size_t>(y0 + y) * w_ + x0;
        for (int x = 0; x < cw; ++x) px[x] = (bits & (0x80 >> x)) ? cell.fg : cell.bg;
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
        if (cell.edges & kEdgeRight)
          for (int y = 0; y < ch; ++y) top[static_cast<size_t>(y) * w_ + cw - 1] = ec;
      }
    }
    // Pixels right of the last whole cell take the last cell's background.
    const int used = g.cols() * cw;
    if (used < w_ && g.cols() > 0) {
      uint16_t bg = g.at(g.cols() - 1, r).bg;
      for (int y = 0; y < ch; ++y) {
        uint16_t* px = frame_ + static_cast<size_t>(y0 + y) * w_;
        for (int x = used; x < w_; ++x) px[x] = bg;
      }
    }
    board::pushRows(frame_, y0, y0 + ch);
  }
  g.clearDirty();
}

}  // namespace rc
