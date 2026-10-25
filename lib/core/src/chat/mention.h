#pragma once

#include <string_view>

namespace rc {

// True when content contains "@username" (ASCII case-insensitive) with a
// non-word character or the string edge on both sides. Word = [A-Za-z0-9_].
bool mentionsUser(std::string_view content, std::string_view username);

}  // namespace rc
