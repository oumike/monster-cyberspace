#include "ui/scale2x.h"

namespace rc::scale2x {

void frameSize(int nw, int nh, uint8_t r, int& w, int& h) {
  if (r & 1) {
    w = nw;
    h = nh;
  } else {
    w = nh;
    h = nw;
  }
}

Pt frameToNative(int x, int y, int nw, int nh, uint8_t r) {
  switch (r & 3) {
    case 1: return {x, y};
    case 3: return {nw - 1 - x, nh - 1 - y};
    case 0: return {nw - 1 - y, x};
    default: return {y, nh - 1 - x};
  }
}

Pt nativeToFrame(int nx, int ny, int nw, int nh, uint8_t r) {
  switch (r & 3) {
    case 1: return {nx, ny};
    case 3: return {nw - 1 - nx, nh - 1 - ny};
    case 0: return {ny, nw - 1 - nx};
    default: return {nh - 1 - ny, nx};
  }
}

void nativeRows(int y0, int y1, int nw, int nh, uint8_t r, int& ny0, int& ny1) {
  (void)nw;
  switch (r & 3) {
    case 1:
      ny0 = y0;
      ny1 = y1;
      break;
    case 3:
      ny0 = nh - y1;
      ny1 = nh - y0;
      break;
    default:  // landscape: a frame row spans every native row
      ny0 = 0;
      ny1 = nh;
      break;
  }
}

static int clampTo(int v, int hi) { return v < 0 ? 0 : (v > hi ? hi : v); }

Pt rawToNative(int rx, int ry, int rawW, int rawH, int nw, int nh) {
  rx = clampTo(rx, rawW - 1);
  ry = clampTo(ry, rawH - 1);
  return {rx * nw / rawW, ry * nh / rawH};
}

}  // namespace rc::scale2x
