#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace rc {

// Encodes text as a QR code (ECC low, smallest version 1–10 that fits) and
// renders it as rows of half-block cells: two modules per cell vertically,
// using '█', '▀', '▄' and ' ', with a one-module quiet zone. Returns false if
// the text doesn't fit version 10.
bool qrToCells(std::string_view text, std::vector<std::u32string>& rows);

}  // namespace rc
