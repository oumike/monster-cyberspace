#include "ui/splash_layout.h"

#include <algorithm>

#include "util/utf8.h"

namespace rc {

namespace {

// Lines share a left edge, so pad nothing on the right.
const char* const kArt[] = {
    R"(            .--''''''--.)",
    R"( _______  .'    o       '.   ______)",
    R"((_______'-'               '-'      '.)",
    R"( '-------.                .-.______.')",
    R"(          '--.._ .-. _..-')",
    R"(             /_/   \_\)",
};
constexpr int kArtRows = sizeof(kArt) / sizeof(kArt[0]);

int width(std::string_view s) { return static_cast<int>(utf8Length(s)); }

std::string clip(std::string_view s, int cols) {
  std::u32string u = utf8ToU32(s);
  if (static_cast<int>(u.size()) > cols) u.resize(std::max(cols, 0));
  return u32ToUtf8(u);
}

}  // namespace

std::vector<SplashLine> splashLayout(int cols, int rows, std::string_view detail) {
  int artW = 0;
  for (const char* l : kArt) artW = std::max(artW, width(l));
  // Art, a blank row, title, detail, a blank row, prompt.
  constexpr int kTextRows = 4;
  bool art = artW <= cols && kArtRows + 1 + kTextRows <= rows;
  int height = art ? kArtRows + 1 + kTextRows : kTextRows;
  int row = std::max((rows - height) / 2, 0);

  std::vector<SplashLine> out;
  if (art) {
    int col = (cols - artW) / 2;
    for (const char* l : kArt) out.push_back({col, row++, l, SplashRole::Art});
    ++row;
  }
  auto centred = [&](std::string_view s, SplashRole role) {
    std::string t = clip(s, cols);
    out.push_back({(cols - width(t)) / 2, row++, t, role});
  };
  centred("Platy for Cyberspace", SplashRole::Title);
  centred(detail, SplashRole::Detail);
  ++row;
  centred("> booting", SplashRole::Prompt);
  return out;
}

}  // namespace rc
