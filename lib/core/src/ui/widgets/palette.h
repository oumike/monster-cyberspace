#pragma once

#include <string>
#include <vector>

namespace rc {

struct PaletteEntry {
  std::u32string label;
  std::u32string insert;  // text put into the input line, e.g. "/me "
};

enum class PaletteKind { Circ, Cmail };

// The command palette: each entry inserts a slash command.
const std::vector<PaletteEntry>& paletteEntries(PaletteKind kind = PaletteKind::Circ);

}  // namespace rc
