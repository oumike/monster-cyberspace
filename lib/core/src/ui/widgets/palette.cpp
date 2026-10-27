#include "ui/widgets/palette.h"

namespace rc {

const std::vector<PaletteEntry>& paletteEntries(PaletteKind kind) {
  static const std::vector<PaletteEntry> entries = [] {
    std::vector<PaletteEntry> v;
    v.push_back({U"Text size", U"", true});
    static const char32_t* const kCmds[] = {
        U"me",      U"slap",  U"poke",  U"hug",   U"hi5",    U"dice",    U"8ball",
        U"fortune", U"gif",   U"art",   U"spoiler", U"blink", U"l33t",   U"comic",
        U"cursive", U"times", U"rainbow", U"flip", U"quiet",  U"slow",    U"glitch",
        U"wave",    U"localtime", U"help", U"muted"};
    for (const char32_t* c : kCmds) {
      std::u32string cmd = U"/" + std::u32string(c);
      v.push_back({cmd, cmd + U" ", false});
    }
    return v;
  }();
  // /art and /muted are cIRC-only: C-Mail answers them with a 400.
  static const std::vector<PaletteEntry> cmail = [] {
    std::vector<PaletteEntry> v;
    for (const auto& e : entries)
      if (e.insert != U"/art " && e.insert != U"/muted ") v.push_back(e);
    return v;
  }();
  return kind == PaletteKind::Cmail ? cmail : entries;
}

}  // namespace rc
