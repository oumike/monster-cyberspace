#include "mascot/mascot.h"

#include <algorithm>

#include "ui/screenshot.h"

namespace rc {

namespace {

constexpr uint8_t kMagic[4] = {'F', 'L', 'U', 'F'};
constexpr uint8_t kVersion = 1;
constexpr size_t kHeader = 10;
constexpr size_t kCrcSize = 4;

bool printable(const std::string& s) {
  for (unsigned char c : s)
    if (c < 0x20 || c > 0x7E) return false;
  return true;
}

}  // namespace

std::vector<uint8_t> encodeMascot(const Mascot& m) {
  std::vector<uint8_t> out(kMascotBlobSize, 0xFF);
  std::vector<uint8_t> payload;
  for (const auto* list : {&m.art, &m.phrases})
    for (const std::string& s : *list) {
      payload.push_back(static_cast<uint8_t>(s.size()));
      payload.insert(payload.end(), s.begin(), s.end());
    }
  const size_t n = payload.size();
  const uint8_t head[kHeader] = {kMagic[0], kMagic[1], kMagic[2], kMagic[3], kVersion,
                                 static_cast<uint8_t>(m.art.size()),
                                 static_cast<uint8_t>(m.phrases.size()), 0,
                                 static_cast<uint8_t>(n), static_cast<uint8_t>(n >> 8)};
  std::copy(head, head + kHeader, out.begin());
  std::copy(payload.begin(), payload.end(), out.begin() + kHeader);
  const uint32_t crc = crc32Update(0, out.data(), kHeader + n);
  for (size_t i = 0; i < kCrcSize; ++i) out[kHeader + n + i] = static_cast<uint8_t>(crc >> (8 * i));
  return out;
}

bool decodeMascot(const uint8_t* data, size_t n, Mascot& out) {
  if (!data || n < kHeader + kCrcSize) return false;
  for (size_t i = 0; i < 4; ++i)
    if (data[i] != kMagic[i]) return false;
  const size_t artCount = data[5], phraseCount = data[6];
  if (data[4] != kVersion || artCount > kMascotArtRows || phraseCount > kMascotMaxPhrases ||
      data[7] != 0)
    return false;
  const size_t len = data[8] | (size_t(data[9]) << 8);
  if (kHeader + len + kCrcSize > n) return false;
  const uint8_t* c = data + kHeader + len;
  const uint32_t stored = c[0] | (uint32_t(c[1]) << 8) | (uint32_t(c[2]) << 16) | (uint32_t(c[3]) << 24);
  if (crc32Update(0, data, kHeader + len) != stored) return false;

  Mascot m;
  const uint8_t* p = data + kHeader;
  const uint8_t* end = p + len;
  auto take = [&](size_t count, size_t minLen, size_t maxLen, std::vector<std::string>& into) {
    for (size_t i = 0; i < count; ++i) {
      if (p >= end) return false;
      const size_t l = *p++;
      if (l < minLen || l > maxLen || l > size_t(end - p)) return false;
      std::string s(reinterpret_cast<const char*>(p), l);
      if (!printable(s)) return false;
      into.push_back(std::move(s));
      p += l;
    }
    return true;
  };
  if (!take(artCount, 0, kMascotArtCols, m.art)) return false;
  if (!take(phraseCount, 1, kMascotPhraseMax, m.phrases)) return false;
  if (p != end) return false;
  out = std::move(m);
  return true;
}

std::string validateMascot(const Mascot& m) {
  if (m.art.size() > kMascotArtRows)
    return "art is over " + std::to_string(kMascotArtRows) + " lines";
  for (size_t i = 0; i < m.art.size(); ++i) {
    const std::string n = std::to_string(i + 1);
    if (m.art[i].size() > kMascotArtCols)
      return "art line " + n + " is over " + std::to_string(kMascotArtCols) + " characters";
    if (!printable(m.art[i])) return "art line " + n + " has a character that isn't plain ASCII";
  }
  if (m.phrases.size() > kMascotMaxPhrases)
    return "over " + std::to_string(kMascotMaxPhrases) + " phrases";
  for (size_t i = 0; i < m.phrases.size(); ++i) {
    const std::string n = std::to_string(i + 1);
    if (m.phrases[i].empty()) return "phrase " + n + " is empty";
    if (m.phrases[i].size() > kMascotPhraseMax)
      return "phrase " + n + " is over " + std::to_string(kMascotPhraseMax) + " characters";
    if (!printable(m.phrases[i])) return "phrase " + n + " has a character that isn't plain ASCII";
  }
  return "";
}

Mascot cleanMascot(Mascot m) {
  for (std::string& l : m.art) {
    const size_t last = l.find_last_not_of(' ');
    l.erase(last == std::string::npos ? 0 : last + 1);
  }
  while (!m.art.empty() && m.art.back().empty()) m.art.pop_back();
  std::vector<std::string> phrases;
  for (std::string& p : m.phrases)
    if (!p.empty()) phrases.push_back(std::move(p));
  m.phrases = std::move(phrases);
  return m;
}

const std::vector<std::string>& defaultMascotArt() {
  // Fluffy, the project's mascot. Lines share a left edge, so pad nothing on
  // the right.
  static const std::vector<std::string> art = {
      R"( /\       /\)",
      R"(/  '-----'  \)",
      R"(|  (o) (o)  |)",
      R"(|   .VVV.   |)",
      R"( \  '^^^'  /)",
      R"(  '-.___.-')",
  };
  return art;
}

const std::vector<std::string>& defaultMascotPhrases() {
  static const std::vector<std::string> words = {
      "boo", "moo", "bop!", "meow", "hi!",
      "We've been trying to reach you about your car's extended warranty.",
  };
  return words;
}

}  // namespace rc
