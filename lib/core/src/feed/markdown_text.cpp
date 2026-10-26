#include "feed/markdown_text.h"

#include <algorithm>
#include <vector>

#include "util/utf8.h"

namespace rc {

namespace {

using U32 = std::u32string;
using U32View = std::u32string_view;

// Bodies are untrusted: nesting deeper than this stays literal, and no search
// for a closing bracket, backtick run or emphasis marker looks further ahead
// than kWindow code points, so a hostile line can't blow the stack or go quadratic.
constexpr int kMaxDepth = 4;
constexpr size_t kWindow = 512;

size_t windowEnd(U32View s, size_t from) { return std::min(s.size(), from + kWindow); }

bool isSpace(char32_t c) { return c == U' ' || c == U'\t'; }

bool isWordChar(char32_t c) {
  return (c >= U'0' && c <= U'9') || (c >= U'a' && c <= U'z') || (c >= U'A' && c <= U'Z') ||
         c >= 0x80;
}

bool isAsciiPunct(char32_t c) {
  return (c >= U'!' && c <= U'/') || (c >= U':' && c <= U'@') || (c >= U'[' && c <= U'`') ||
         (c >= U'{' && c <= U'~');
}

// Finds the closing `close` for an opening bracket at `open`, allowing nesting.
size_t findClose(U32View s, size_t open, char32_t openCh, char32_t close) {
  int depth = 0;
  const size_t end = windowEnd(s, open);
  for (size_t i = open; i < end; ++i) {
    if (s[i] == U'\\') {
      ++i;
    } else if (s[i] == openCh) {
      ++depth;
    } else if (s[i] == close && --depth == 0) {
      return i;
    }
  }
  return U32View::npos;
}

void inlineText(U32View s, U32& out, int depth);

// A link or image starting at `i` ('[' or "!["). On success appends the text and moves `i` past it.
bool linkOrImage(U32View s, size_t& i, U32& out, int depth) {
  const bool image = s[i] == U'!';
  const size_t open = image ? i + 1 : i;
  const size_t closeText = findClose(s, open, U'[', U']');
  if (closeText == U32View::npos || closeText + 1 >= s.size() || s[closeText + 1] != U'(') {
    return false;
  }
  const size_t closeUrl = findClose(s, closeText + 1, U'(', U')');
  if (closeUrl == U32View::npos) return false;
  const U32View text = s.substr(open + 1, closeText - open - 1);
  const U32View url = s.substr(closeText + 2, closeUrl - closeText - 2);
  if (image) {
    U32 alt;
    inlineText(text, alt, depth + 1);
    out += alt.empty() ? U"[img]" : U"[img: " + alt + U"]";
  } else {
    inlineText(text, out, depth + 1);
    out += U" <";
    out += url;
    out += U">";
  }
  i = closeUrl + 1;
  return true;
}

// True if a run of exactly `run` × `c` starts at j.
bool exactRun(U32View s, size_t j, char32_t c, size_t run) {
  if (j > 0 && s[j - 1] == c) return false;
  for (size_t k = 0; k < run; ++k) {
    if (j + k >= s.size() || s[j + k] != c) return false;
  }
  return j + run == s.size() || s[j + run] != c;
}

// The closing marker for an opener of `run` × `c` that ends at `from`, or npos.
size_t findCloser(U32View s, size_t from, char32_t c, size_t run) {
  const size_t end = windowEnd(s, from);
  for (size_t j = from + 1; j < end; ++j) {
    if (s[j - 1] == U'\\') continue;
    if (!isSpace(s[j - 1]) && exactRun(s, j, c, run)) return j;
  }
  return U32View::npos;
}

// One left-to-right scan of a line's inline markup.
void inlineText(U32View s, U32& out, int depth) {
  std::vector<size_t> closers;  // positions of emphasis markers already paired with an opener
  size_t i = 0;
  while (i < s.size()) {
    const char32_t c = s[i];
    if (c == U'\\' && i + 1 < s.size() && isAsciiPunct(s[i + 1])) {
      out += s[i + 1];
      i += 2;
      continue;
    }
    if (c == U'`') {
      size_t run = 0;
      while (i + run < s.size() && s[i + run] == U'`') ++run;
      const U32 fence(run, U'`');
      const size_t end = s.substr(0, windowEnd(s, i + run)).find(fence, i + run);
      if (end == U32View::npos) {
        out.append(fence);
        i += run;
      } else {
        out.append(s.substr(i + run, end - i - run));
        i = end + run;
      }
      continue;
    }
    if ((c == U'[' || (c == U'!' && i + 1 < s.size() && s[i + 1] == U'[')) &&
        depth < kMaxDepth && linkOrImage(s, i, out, depth)) {
      continue;
    }
    if (c == U'*' || c == U'_' || c == U'~') {
      size_t run = 1;
      if (i + 1 < s.size() && s[i + 1] == c) run = 2;
      if (c == U'~' && run == 1) {
        out += c;
        ++i;
        continue;
      }
      // Drop closers the scan skipped (inside code or a link) so the list stays short.
      closers.erase(std::remove_if(closers.begin(), closers.end(), [i](size_t p) { return p < i; }),
                    closers.end());
      const auto closer = std::find(closers.begin(), closers.end(), i);
      if (closer != closers.end()) {
        closers.erase(closer);
        i += run;
        continue;
      }
      const char32_t prev = i > 0 ? s[i - 1] : U' ';
      const char32_t next = i + run < s.size() ? s[i + run] : U' ';
      // A marker is dropped only if it opens a pair closed later on the line.
      // One with space on both sides (as in "2 * 3") or after it, or an
      // intraword '_' (as in snake_case), is literal.
      size_t close = U32View::npos;
      if (!isSpace(next) && !(c == U'_' && isWordChar(prev) && isWordChar(next))) {
        close = findCloser(s, i + run, c, run);
      }
      if (close == U32View::npos) {
        out.append(run, c);
      } else {
        closers.push_back(close);
      }
      i += run;
      continue;
    }
    out += c;
    ++i;
  }
}

bool isFence(U32View line) {
  size_t i = 0;
  while (i < line.size() && i < 3 && line[i] == U' ') ++i;
  const U32View rest = line.substr(i);
  return rest.substr(0, 3) == U"```" || rest.substr(0, 3) == U"~~~";
}

// A thematic break such as "***" or "_ _ _".
bool isRule(U32View line) {
  size_t marks = 0;
  char32_t mark = 0;
  for (char32_t c : line) {
    if (isSpace(c)) continue;
    if ((c != U'*' && c != U'_') || (mark && c != mark)) return false;
    mark = c;
    ++marks;
  }
  return marks >= 3;
}

bool isBlank(U32View line) {
  for (char32_t c : line) {
    if (!isSpace(c)) return false;
  }
  return true;
}

// Rewrites block markers at the start of a line, then the inline markup.
U32 blockLine(U32View line) {
  U32 out;
  size_t i = 0;
  while (i < line.size() && isSpace(line[i])) out += line[i++];

  size_t hashes = 0;
  while (i + hashes < line.size() && line[i + hashes] == U'#') ++hashes;
  if (hashes >= 1 && hashes <= 6 && (i + hashes == line.size() || isSpace(line[i + hashes]))) {
    i += hashes;
    while (i < line.size() && isSpace(line[i])) ++i;
  }

  while (i < line.size() && line[i] == U'>') {
    out += U"│";
    ++i;
    if (i < line.size() && line[i] == U' ') {
      out += U' ';
      ++i;
    }
  }

  if (i + 1 < line.size() && (line[i] == U'-' || line[i] == U'*' || line[i] == U'+') &&
      line[i + 1] == U' ') {
    out += U"• ";
    i += 2;
  }

  inlineText(line.substr(i), out, 0);
  return out;
}

}  // namespace

std::u32string markdownToText(std::string_view md) {
  const U32 all = utf8ToU32(md);
  std::vector<U32> lines;
  bool fenced = false;
  bool lastBlank = true;  // drops leading blank lines
  size_t start = 0;
  while (start <= all.size()) {
    size_t end = all.find(U'\n', start);
    if (end == U32::npos) end = all.size();
    U32View line(all.data() + start, end - start);
    if (!line.empty() && line.back() == U'\r') line.remove_suffix(1);
    start = end + 1;

    if (isFence(line)) {
      fenced = !fenced;
      continue;
    }
    if (fenced) {
      lines.emplace_back(line);
      lastBlank = false;
      continue;
    }
    U32 text = isBlank(line) || isRule(line) ? U32() : blockLine(line);
    if (isBlank(text)) {
      if (!lastBlank) lines.emplace_back();
      lastBlank = true;
      continue;
    }
    lines.push_back(std::move(text));
    lastBlank = false;
  }
  while (!lines.empty() && lines.back().empty()) lines.pop_back();

  U32 out;
  for (size_t i = 0; i < lines.size(); ++i) {
    if (i) out += U'\n';
    out += lines[i];
  }
  return out;
}

bool truncateUtf8(std::string& s, size_t maxBytes) {
  if (s.size() <= maxBytes) return false;
  size_t cut = maxBytes;
  while (cut > 0 && (static_cast<unsigned char>(s[cut]) & 0xC0) == 0x80) --cut;
  s.resize(cut);
  return true;
}

bool truncateCodePoints(std::string& s, size_t n) {
  size_t count = 0;
  for (size_t i = 0; i < s.size(); ++i) {
    if ((static_cast<unsigned char>(s[i]) & 0xC0) == 0x80) continue;
    if (count == n) {
      s.resize(i);
      return true;
    }
    ++count;
  }
  return false;
}

int nextSmallerLimit(int limit) {
  if (limit > 10) return 10;
  if (limit > 5) return 5;
  if (limit > 2) return 2;
  return 0;
}

}  // namespace rc
