#pragma once

#include <string>
#include <string_view>

namespace rc {

// Standard alphabet with padding optional. Line breaks are skipped; any other
// character outside the alphabet makes the decode fail.
bool base64Decode(std::string_view in, std::string& out);

}  // namespace rc
