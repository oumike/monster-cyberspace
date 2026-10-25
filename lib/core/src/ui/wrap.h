#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace rc {

// Wraps text into lines of at most firstWidth cells for the first line and
// contWidth cells after that (hanging indent). Each code point is one cell.
// Breaks after spaces, hard-splits words longer than the width, and forces a
// break at '\n'. Trailing spaces at a break are dropped. Empty text yields
// one empty line. Widths below 1 are treated as 1.
std::vector<std::u32string> wrapText(std::u32string_view text, int firstWidth,
                                     int contWidth);

}  // namespace rc
