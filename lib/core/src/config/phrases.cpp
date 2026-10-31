#include "config/phrases.h"

namespace rc {

std::vector<std::string> cleanPhrases(const std::vector<std::string>& in) {
  std::vector<std::string> out;
  for (const std::string& raw : in) {
    if (out.size() == kMaxPhrases) break;
    std::string s;
    for (char c : raw) s += (static_cast<unsigned char>(c) < 0x20 || c == 0x7F) ? ' ' : c;
    std::string t;
    for (char c : s) {
      if (c == ' ' && (t.empty() || t.back() == ' ')) continue;
      t += c;
    }
    while (!t.empty() && t.back() == ' ') t.pop_back();
    if (t.size() > kMaxPhraseBytes) {
      size_t n = kMaxPhraseBytes;
      while (n > 0 && (static_cast<unsigned char>(t[n]) & 0xC0) == 0x80) --n;
      t.resize(n);
      while (!t.empty() && t.back() == ' ') t.pop_back();
    }
    if (!t.empty()) out.push_back(std::move(t));
  }
  return out;
}

std::string joinPhrases(const std::vector<std::string>& p) {
  std::string o;
  for (size_t i = 0; i < p.size(); ++i) {
    if (i) o += '\n';
    o += p[i];
  }
  return o;
}

std::vector<std::string> splitPhrases(std::string_view text) {
  std::vector<std::string> lines;
  while (!text.empty()) {
    const size_t nl = text.find('\n');
    lines.emplace_back(text.substr(0, nl));
    if (nl == std::string_view::npos) break;
    text.remove_prefix(nl + 1);
  }
  return cleanPhrases(lines);
}

std::vector<std::string> defaultPhrases() {
  return {"On my way", "Sounds good", "Thanks!", "Can't talk now, later", "lol", "brb"};
}

}  // namespace rc
