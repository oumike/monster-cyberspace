#include "ui/qr_cells.h"

#include <qrcode.h>

#include <cstdint>
#include <vector>

namespace rc {

bool qrToCells(std::string_view text, std::vector<std::u32string>& rows) {
  // Byte-mode capacity at ECC_LOW for versions 1–10. The library doesn't
  // check capacity itself, so pick the version here.
  static const uint16_t kCap[] = {17, 32, 53, 78, 106, 134, 154, 192, 230, 271};
  uint8_t version = 0;
  for (uint8_t v = 1; v <= 10; ++v) {
    if (text.size() <= kCap[v - 1]) {
      version = v;
      break;
    }
  }
  if (version == 0) return false;

  std::string data(text);
  QRCode qr;
  std::vector<uint8_t> buf(qrcode_getBufferSize(version));
  if (qrcode_initText(&qr, buf.data(), version, ECC_LOW, data.c_str()) != 0) return false;

  const int n = qr.size + 2;  // with quiet zone
  auto dark = [&](int x, int y) {
    x -= 1;
    y -= 1;
    if (x < 0 || y < 0 || x >= qr.size || y >= qr.size) return false;
    return qrcode_getModule(&qr, x, y);
  };
  rows.clear();
  for (int y = 0; y < n; y += 2) {
    std::u32string row;
    for (int x = 0; x < n; ++x) {
      bool top = dark(x, y), bottom = y + 1 < n && dark(x, y + 1);
      row += top && bottom ? U'\u2588' : top ? U'\u2580' : bottom ? U'\u2584' : U' ';
    }
    rows.push_back(row);
  }
  return true;
}

}  // namespace rc
