#pragma once

#include <string>
#include <vector>

namespace rc {

struct PaletteEntry {
  std::u32string label;
  std::u32string insert;  // text put into the input line, e.g. "/me "
  bool isTextSize = false;
};

// The command palette. The first entry toggles the text size; the rest
// insert a slash command.
const std::vector<PaletteEntry>& paletteEntries();

}  // namespace rc
