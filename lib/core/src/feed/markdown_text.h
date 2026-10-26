#pragma once

#include <string>
#include <string_view>

namespace rc {

// Plain text with '\n' line breaks, ready for wrapText().
std::u32string markdownToText(std::string_view md);
// Cuts s to at most maxBytes without splitting a UTF-8 sequence. True if it cut.
bool truncateUtf8(std::string& s, size_t maxBytes);
// Cuts to at most n code points. True if it cut.
bool truncateCodePoints(std::string& s, size_t n);
// 10 → 5 → 2 → 0 (0 = give up).
int nextSmallerLimit(int limit);

}  // namespace rc
