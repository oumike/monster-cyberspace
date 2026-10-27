#include "input/touch_transform.h"

namespace rc {

void touchToFrame(int16_t rx, int16_t ry, int nativeW, int nativeH, uint8_t r,
                  int16_t& x, int16_t& y) {
  if (rx < 0) rx = 0;
  if (rx >= nativeW) rx = (int16_t)(nativeW - 1);
  if (ry < 0) ry = 0;
  if (ry >= nativeH) ry = (int16_t)(nativeH - 1);

  const int W = nativeW, H = nativeH;
  const int lx = H - 1 - ry;  // today's gt911::toLandscape
  const int ly = rx;

  switch (r & 3) {
    case 0:
      x = (int16_t)lx;
      y = (int16_t)ly;
      break;
    case 1:
      x = (int16_t)(W - 1 - ly);
      y = (int16_t)lx;
      break;
    case 2:
      x = (int16_t)(H - 1 - lx);
      y = (int16_t)(W - 1 - ly);
      break;
    default:  // 3
      x = (int16_t)ly;
      y = (int16_t)(H - 1 - lx);
      break;
  }
}

}  // namespace rc
