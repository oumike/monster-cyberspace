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

}  // namespace rc
