#include "ui/splash_layout.h"

#include <algorithm>

#include "ui/wrap.h"
#include "util/utf8.h"

namespace rc {

namespace {

// Fluffy, the project's mascot. Lines share a left edge, so pad nothing on
// the right.
const char* const kArt[] = {
    R"( /\       /\)",
    R"(/  '-----'  \)",
    R"(|  (o) (o)  |)",
    R"(|   .VVV.   |)",
    R"( \  '^^^'  /)",
    R"(  '-.___.-')",
};
constexpr int kArtRows = sizeof(kArt) / sizeof(kArt[0]);

int width(std::string_view s) { return static_cast<int>(utf8Length(s)); }

std::string clip(std::string_view s, int cols) {
  std::u32string u = utf8ToU32(s);
  if (static_cast<int>(u.size()) > cols) u.resize(std::max(cols, 0));
  return u32ToUtf8(u);
}

// The widest text line in a bubble, so a long one stays a bubble, not a bar.
constexpr int kBubbleText = 18;

// The bubble's rows: a border, the text with the tail on its first line, and
// a border. Empty when it can't get `maxW` cells or fewer. tailDown: the tail
// goes on the bottom border instead, near the left.
std::vector<std::string> bubbleRows(std::string_view say, int maxW, bool tailDown = false) {
  const int inner = std::min(kBubbleText, maxW - 4);
  if (say.empty() || inner < 3) return {};
  std::vector<std::u32string> lines = wrapText(utf8ToU32(say), inner, inner);
  int w = 0;
  for (const auto& l : lines) w = std::max(w, static_cast<int>(l.size()));
  std::vector<std::string> out;
  out.push_back("." + std::string(w + 2, '-') + ".");
  for (size_t i = 0; i < lines.size(); ++i) {
    std::u32string l = lines[i];
    l.resize(w, U' ');
    out.push_back(std::string(i == 0 && !tailDown ? "< " : "| ") + u32ToUtf8(l) + " |");
  }
  out.push_back(tailDown ? "'-v" + std::string(w, '-') + "'" : "'" + std::string(w + 2, '-') + "'");
  return out;
}

}  // namespace

const char* const kMonsterWords[kMonsterWordCount] = {
    "boo", "moo", "bop!", "meow", "hi!",
    "We've been trying to reach you about your car's extended warranty.",
};

const char* MonsterWords::next(uint32_t rnd) {
  if (pos_ >= kMonsterWordCount) {
    for (int i = 0; i < kMonsterWordCount; ++i) order_[i] = i;
    uint32_t r = rnd | 1;
    for (int i = kMonsterWordCount - 1; i > 0; --i) {
      r ^= r << 13;
      r ^= r >> 17;
      r ^= r << 5;
      std::swap(order_[i], order_[r % uint32_t(i + 1)]);
    }
    if (order_[0] == last_) std::swap(order_[0], order_[kMonsterWordCount - 1]);
    pos_ = 0;
  }
  last_ = order_[pos_++];
  return kMonsterWords[last_];
}

std::vector<SplashLine> splashLayout(int cols, int rows, std::string_view detail,
                                     std::string_view say, int maxScale) {
  int artW = 0;
  for (const char* l : kArt) artW = std::max(artW, width(l));
  // Art (and its bubble), a blank row, title, detail, a blank row, prompt.
  constexpr int kTextRows = 4;
  // A scaled monster: the biggest that leaves a margin each side and room
  // below for the text. Its rows each take scale rows of the panel.
  for (int s = maxScale; s >= 2; --s) {
    if (artW * s > cols - 4 || kArtRows * s + 1 + kTextRows > rows) continue;
    const int artCol = (cols / s - artW) / 2;  // in scaled cells
    // The bubble's tail over the monster's right side.
    const int bubCol = artCol * s + artW * s * 6 / 10 - 2;
    std::vector<std::string> bub = bubbleRows(say, cols - bubCol, true);
    if (int(bub.size()) + kArtRows * s + 1 + kTextRows > rows) bub.clear();
    int row = std::max((rows - (int(bub.size()) + kArtRows * s + 1 + kTextRows)) / 2, 0);
    std::vector<SplashLine> out;
    for (const std::string& b : bub) out.push_back({bubCol, row++, b, SplashRole::Bubble});
    for (int i = 0; i < kArtRows; ++i) out.push_back({artCol, row++, kArt[i], SplashRole::Art, s});
    ++row;
    auto centred = [&](std::string_view t, SplashRole role) {
      std::string c = clip(t, cols);
      out.push_back({(cols - width(c)) / 2, row++, c, role});
    };
    centred("Monster for Cyberspace", SplashRole::Title);
    centred(detail, SplashRole::Detail);
    ++row;
    centred("> booting", SplashRole::Prompt);
    return out;
  }
  bool art = artW <= cols && kArtRows + 1 + kTextRows <= rows;
  std::vector<std::string> bub;
  if (art) {
    bub = bubbleRows(say, cols - artW - 1);
    if (std::max(kArtRows, int(bub.size())) + 1 + kTextRows > rows) bub.clear();
  }
  const int bubW = bub.empty() ? 0 : width(bub[0]);
  const int artH = std::max(kArtRows, int(bub.size()));
  int height = art ? artH + 1 + kTextRows : kTextRows;
  int row = std::max((rows - height) / 2, 0);

  std::vector<SplashLine> out;
  if (art) {
    const int blockW = artW + (bubW ? 1 + bubW : 0);
    const int col = (cols - blockW) / 2;
    for (int i = 0; i < kArtRows; ++i) out.push_back({col, row + i, kArt[i], SplashRole::Art});
    for (int i = 0; i < int(bub.size()); ++i)
      out.push_back({col + artW + 1, row + i, bub[i], SplashRole::Bubble});
    row += artH + 1;
  }
  auto centred = [&](std::string_view s, SplashRole role) {
    std::string t = clip(s, cols);
    out.push_back({(cols - width(t)) / 2, row++, t, role});
  };
  centred("Monster for Cyberspace", SplashRole::Title);
  centred(detail, SplashRole::Detail);
  ++row;
  centred("> booting", SplashRole::Prompt);
  return out;
}

}  // namespace rc
