#include "screens/link_view.h"

#include "screens/draw.h"
#include "ui/qr_cells.h"
#include "ui/wrap.h"

namespace rc {

void LinkView::open(const std::string& url) {
  url_ = utf8ToU32(url);
  qr_.clear();
  if (!qrToCells(url, qr_)) qr_.clear();
}

void LinkView::render(Grid& g, const Theme& t) const {
  const int bottom = g.rows() - 1;
  int r = 1;
  for (const auto& line : wrapText(url_, g.cols() - 2, g.cols() - 2))
    if (r < bottom) g.putText(1, r++, line, t.fg, t.bg, g.cols() - 2);
  int qrH = int(qr_.size()) + 2, qrW = qr_.empty() ? 0 : int(qr_[0].size()) + 4;
  if (!qr_.empty() && r + qrH <= bottom && qrW <= g.cols()) {
    int left = (g.cols() - qrW) / 2;
    // Dark modules on white with a light margin, so phones can scan it.
    for (int y = 0; y < qrH; ++y)
      for (int x = 0; x < qrW; ++x) g.put(left + x, r + y, U' ', 0x0000, 0xFFFF);
    for (int y = 0; y < int(qr_.size()); ++y)
      g.putText(left + 2, r + 1 + y, qr_[y], 0x0000, 0xFFFF, qrW - 4);
  } else if (!qr_.empty() || r < bottom) {
    if (r + 1 < bottom) putLine(g, 1, r + 1, "(no room for a QR code here)", t.dim, t.bg);
  }
  drawHint(g, t, U"back: close");
}

}  // namespace rc
