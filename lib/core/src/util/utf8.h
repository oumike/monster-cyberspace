#pragma once

#include <string>
#include <string_view>

namespace rc {

// Decodes UTF-8 to code points. An invalid or truncated sequence becomes
// U+FFFD and consumes one byte, so decoding always makes progress.
std::u32string utf8ToU32(std::string_view s);
std::string u32ToUtf8(std::u32string_view s);
// Number of code points.
size_t utf8Length(std::string_view s);

}  // namespace rc
