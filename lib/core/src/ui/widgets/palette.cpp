#include "ui/widgets/palette.h"

#include <cstdio>

namespace rc {

namespace {

struct Cmd {
  const char32_t* name;
  const char32_t* hint;
  bool circOnly;
};

constexpr Cmd kCmds[] = {
    {U"me", U"an action: /me waves", false},
    {U"poke", U"at @user, or alone", false},
    {U"hug", U"at @user, or alone", false},
    {U"hi5", U"at @user, or alone", false},
    {U"slap", U"at @user, or alone", false},
    {U"dice", U"roll: 2d6+3, 4d6kh3, adv", false},
    {U"coin", U"heads or tails", false},
    {U"8ball", U"ask a question", false},
    {U"fortune", U"a fortune cookie", false},
    {U"gif", U"a GIF by https link", false},
    {U"song", U"YouTube link | artist | title (supporters)", false},
    {U"art", U"ASCII art on the lines after", true},
    {U"spoiler", U"hidden until clicked", false},
    {U"blink", U"text style; chain with +", false},
    {U"l33t", U"text style; chain with +", false},
    {U"comic", U"text style; chain with +", false},
    {U"cursive", U"text style; chain with +", false},
    {U"times", U"text style; chain with +", false},
    {U"rainbow", U"text style; chain with +", false},
    {U"flip", U"text style; chain with +", false},
    {U"quiet", U"text style; chain with +", false},
    {U"slow", U"text style; chain with +", false},
    {U"glitch", U"text style; chain with +", false},
    {U"wave", U"text style; chain with +", false},
    {U"localtime", U"your time, from the clock", true},
    {U"mute", U"mute @user in this room", true},
    {U"unmute", U"unmute @user here", true},
    {U"muted", U"list your mutes here", true},
    {U"unmuteall", U"clear your mutes here", true},
    {U"help", U"the server's command list", false},
};

char32_t lower(char32_t c) { return c >= U'A' && c <= U'Z' ? c + 32 : c; }

}  // namespace

const std::vector<PaletteEntry>& paletteEntries(PaletteKind kind) {
  static const std::vector<PaletteEntry> circ = [] {
    std::vector<PaletteEntry> v;
    for (const Cmd& c : kCmds) {
      std::u32string cmd = U"/" + std::u32string(c.name);
      v.push_back({cmd, cmd + U" ", c.hint, c.circOnly});
    }
    return v;
  }();
  // C-Mail answers the cIRC-only ones with a 400.
  static const std::vector<PaletteEntry> cmail = [] {
    std::vector<PaletteEntry> v;
    for (const auto& e : circ)
      if (!e.circOnly) v.push_back(e);
    return v;
  }();
  return kind == PaletteKind::Cmail ? cmail : circ;
}

std::vector<const PaletteEntry*> paletteMatches(PaletteKind kind, const std::u32string& typed) {
  std::vector<const PaletteEntry*> out;
  if (typed.empty() || typed[0] != U'/') return out;
  for (char32_t c : typed)
    if (c == U' ' || c == U'\n' || c == U'\t') return out;
  for (const auto& e : paletteEntries(kind)) {
    if (e.label.size() < typed.size()) continue;
    bool ok = true;
    for (size_t i = 0; i < typed.size() && ok; ++i) ok = lower(e.label[i]) == lower(typed[i]);
    if (ok) out.push_back(&e);
  }
  return out;
}

std::u32string paletteInsert(const PaletteEntry& e, const std::tm* clock) {
  if (e.label != U"/localtime" || !clock) return e.insert;
  char buf[8];
  snprintf(buf, sizeof buf, "%02d:%02d", clock->tm_hour % 24, clock->tm_min % 60);
  std::u32string out = U"/localtime ";
  for (const char* p = buf; *p; ++p) out += char32_t(*p);
  return out + U" ";
}

}  // namespace rc
