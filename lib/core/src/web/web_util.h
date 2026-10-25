#pragma once

#include <string>
#include <string_view>

namespace rc {

// "Rhino-Cyberspace-<board>-Setup".
std::string setupApSsid(std::string_view boardName);

// Empty means an open AP. Otherwise WPA2 rules: 8-63 printable ASCII.
bool validApPassword(std::string_view pw);

// HTTP Basic auth for the config page, user "admin". An empty password
// disables the check.
bool checkBasicAuth(std::string_view authorizationHeader, std::string_view password);

// Decodes a query-string value: %XX escapes and '+' as a space. A malformed
// escape is kept as-is.
std::string urlDecode(std::string_view s);

}  // namespace rc
