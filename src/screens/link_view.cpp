#include "screens/link_view.h"

#include "screens/lv_draw.h"
#include "ui/qr_cells.h"
#include "ui/wrap.h"

namespace rc {

void LinkView::open(const std::string& url) {
  url_ = utf8ToU32(url);
  urlUtf8_ = url;
  qr_.clear();
  if (!qrToCells(url, qr_)) qr_.clear();
}

void LinkView::sync(lvui::CellText& text, lvui::QrBox& qr, lvui::HintBar& hint) const {
  const Theme& t = lvui::currentTheme();
  const int cols = text.cols(), bottom = text.rows() - 1;
  int r = 1;
  for (const auto& line : wrapText(url_, cols - 2, cols - 2))
    if (r < bottom) text.line(1, r++, line, t.fg, cols - 2);
  // QrBox encodes the URL as open() did into qr_: the same rows, plus the margins.
  const bool encoded = !qr_.empty() && qr.sync(urlUtf8_);
  const int qrH = qr.rows(), qrW = qr.cols();
  const bool fits = encoded && r + qrH <= bottom && qrW <= cols;
  lvui::setHidden(qr.obj(), !fits);
  if (fits) {
    const lvui::Cell c = text.cell();
    lvui::place(qr.obj(), (cols - qrW) / 2 * c.w, r * c.h, qrW * c.w, qrH * c.h);
  } else if (!qr_.empty() || r < bottom) {
    if (r + 1 < bottom) text.line(1, r + 1, "(no room for a QR code here)", t.dim);
  }
  syncHint(hint, U"back: close", U"\u2039 close");
}

}  // namespace rc
