#pragma once

#include <ctime>
#include <string>
#include <vector>

namespace rc {

struct PaletteEntry {
  std::u32string label;   // "/me"
  std::u32string insert;  // text put into the input line, e.g. "/me "
  std::u32string hint;    // one line on what it does
  bool circOnly = false;
};

enum class PaletteKind { Circ, Cmail };

// The slash commands (api docs, Commands); C-Mail leaves out the cIRC-only ones.
const std::vector<PaletteEntry>& paletteEntries(PaletteKind kind = PaletteKind::Circ);

// The entries whose command starts with typed (case-insensitive), in table
// order. typed must be one word starting with '/'; "/" alone matches all.
std::vector<const PaletteEntry*> paletteMatches(PaletteKind kind, const std::u32string& typed);

// The text a pick puts in the input: insert, or for /localtime the time from
// clock ("/localtime 09:05 "), or bare without a clock.
std::u32string paletteInsert(const PaletteEntry& e, const std::tm* clock);

}  // namespace rc
