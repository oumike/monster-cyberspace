#include "ui/scale_blit.h"

#include <cstddef>

using std::size_t;

namespace rc {

BlitRect scaleBlitRows(const uint16_t* frame, int fw, int fh, int y0, int y1, uint8_t r, int scale,
                       uint16_t* out) {
  r &= 3;
  if (y0 < 0) y0 = 0;
  if (y1 > fh) y1 = fh;
  const int band = y1 > y0 ? y1 - y0 : 0;
  // Native portrait size, in frame pixels.
  const int W = (r & 1) ? fw : fh, H = (r & 1) ? fh : fw;
  BlitRect n{};
  switch (r) {
    case 0: n = {y0, 0, band, H}; break;
    case 1: n = {0, H - y1, W, band}; break;
    case 2: n = {W - y1, 0, band, H}; break;
    default: n = {0, y0, W, band}; break;
  }
  const int ow = n.w * scale;
  for (int ny = n.y; ny < n.y + n.h; ++ny) {
    uint16_t* row = out + static_cast<size_t>(ny - n.y) * scale * ow;
    for (int nx = n.x; nx < n.x + n.w; ++nx) {
      // touchToFrame, for in-range native coordinates.
      const int lx = H - 1 - ny, ly = nx;
      int x, y;
      switch (r) {
        case 0: x = lx; y = ly; break;
        case 1: x = W - 1 - ly; y = lx; break;
        case 2: x = H - 1 - lx; y = W - 1 - ly; break;
        default: x = ly; y = H - 1 - lx; break;
      }
      const uint16_t px = frame[static_cast<size_t>(y) * fw + x];
      uint16_t* p = row + (nx - n.x) * scale;
      for (int k = 0; k < scale; ++k) p[k] = px;
    }
    for (int k = 1; k < scale; ++k) {
      uint16_t* dst = row + static_cast<size_t>(k) * ow;
      for (int i = 0; i < ow; ++i) dst[i] = row[i];
    }
  }
  return {n.x * scale, n.y * scale, ow, n.h * scale};
}

}  // namespace rc
