#include "ui/wrap.h"

namespace rc {

namespace {

void wrapParagraph(std::u32string_view para, size_t firstW, size_t contW,
                   std::vector<std::u32string>& out) {
  if (para.empty()) {
    out.emplace_back();
    return;
  }
  size_t i = 0;
  while (i < para.size()) {
    size_t w = out.empty() ? firstW : contW;
    std::u32string_view rest = para.substr(i);
    if (rest.size() <= w) {
      out.emplace_back(rest);
      return;
    }
    // A space at index w is fine: the w cells before it fit.
    size_t brk = rest.find_last_of(U' ', w);
    size_t end = brk;
    while (end != std::u32string_view::npos && end > 0 && rest[end - 1] == U' ')
      --end;
    if (brk == std::u32string_view::npos || end == 0) {
      out.emplace_back(rest.substr(0, w));
      i += w;
    } else {
      out.emplace_back(rest.substr(0, end));
      i += brk + 1;
    }
    while (i < para.size() && para[i] == U' ') ++i;
  }
}

}  // namespace

std::vector<std::u32string> wrapText(std::u32string_view text, int firstWidth,
                                     int contWidth) {
  size_t firstW = firstWidth < 1 ? 1 : static_cast<size_t>(firstWidth);
  size_t contW = contWidth < 1 ? 1 : static_cast<size_t>(contWidth);
  std::vector<std::u32string> out;
  size_t start = 0;
  while (true) {
    size_t nl = text.find(U'\n', start);
    wrapParagraph(text.substr(start, nl == std::u32string_view::npos
                                         ? std::u32string_view::npos
                                         : nl - start),
                  firstW, contW, out);
    if (nl == std::u32string_view::npos) break;
    start = nl + 1;
  }
  return out;
}

}  // namespace rc
