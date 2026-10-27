#pragma once

#include <string>
#include <string_view>

namespace rc {

// Double-quoted, with \\ \" \n \t and \uXXXX for other bytes < 0x20 and 0x7F.
// Other UTF-8 (accents, emoji) is written as-is.
std::string yamlQuote(std::string_view s);

enum class ScalarKind { Plain, Double, Single };

// Reads the scalar that makes up the rest of a line (after "key: ").
// Leading and trailing spaces are dropped; a plain scalar ends at the first
// " #"; after a closing quote only spaces or " # comment" may follow.
// false with err (no "line N: " prefix) on a bad escape, an unclosed quote,
// or junk after the closing quote.
bool yamlScalar(std::string_view rest, std::string& out, ScalarKind& kind, std::string& err);

}  // namespace rc
