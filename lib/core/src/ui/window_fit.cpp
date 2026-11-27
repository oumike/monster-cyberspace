// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/window_fit.h"

namespace rc::winfit {

int autoZoom(int drawW, int drawH) {
  for (int z = kAutoZoomMax; z > 1; --z)
    if (drawW / z >= kAutoPanelW && drawH / z >= kAutoPanelH) return z;
  return 1;
}

Fit fit(int drawW, int drawH, int zoom) {
  Fit f;
  f.zoom = zoom == kZoomAuto ? autoZoom(drawW, drawH) : clampZoom(zoom);
  f.panelW = drawW / f.zoom > 0 ? drawW / f.zoom : 1;
  f.panelH = drawH / f.zoom > 0 ? drawH / f.zoom : 1;
  const int spareX = drawW - f.panelW * f.zoom, spareY = drawH - f.panelH * f.zoom;
  f.offX = spareX > 0 ? spareX / 2 : 0;
  f.offY = spareY > 0 ? spareY / 2 : 0;
  return f;
}

bool toPanel(const Fit& f, int drawX, int drawY, int16_t& x, int16_t& y) {
  const int rx = drawX - f.offX, ry = drawY - f.offY;
  const bool inside = rx >= 0 && ry >= 0 && rx < f.panelW * f.zoom && ry < f.panelH * f.zoom;
  int px = rx < 0 ? 0 : rx / f.zoom, py = ry < 0 ? 0 : ry / f.zoom;
  if (px >= f.panelW) px = f.panelW - 1;
  if (py >= f.panelH) py = f.panelH - 1;
  x = int16_t(px);
  y = int16_t(py);
  return inside;
}

void minWindow(int lcdW, int lcdH, int zoom, int& w, int& h) {
  const int z = clampZoom(zoom);
  w = lcdW * z;
  h = lcdH * z;
}

}  // namespace rc::winfit
