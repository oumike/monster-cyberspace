#include "web/web_util.h"

#include "util/base64.h"

namespace rc {

std::string setupApSsid(std::string_view boardName) {
  std::string s = "Rhino-Cyberspace-";
  s.append(boardName);
  s += "-Setup";
  return s;
}

bool validApPassword(std::string_view pw) {
  if (pw.empty()) return true;
  if (pw.size() < 8 || pw.size() > 63) return false;
  for (char c : pw) {
    if (c < 0x20 || c > 0x7E) return false;
  }
  return true;
}

bool checkBasicAuth(std::string_view header, std::string_view password) {
  if (password.empty()) return true;
  constexpr std::string_view kScheme = "basic ";
  if (header.size() <= kScheme.size()) return false;
  for (size_t i = 0; i < kScheme.size(); ++i) {
    char c = header[i];
    if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
    if (c != kScheme[i]) return false;
  }
  std::string_view b64 = header.substr(kScheme.size());
  while (!b64.empty() && b64.front() == ' ') b64.remove_prefix(1);
  std::string decoded;
  if (!base64Decode(b64, decoded)) return false;
  std::string expected = "admin:";
  expected.append(password);
  // Length-independent compare over the expected string.
  unsigned diff = decoded.size() != expected.size();
  for (size_t i = 0; i < expected.size(); ++i) {
    char d = i < decoded.size() ? decoded[i] : 0;
    diff |= static_cast<unsigned char>(d ^ expected[i]);
  }
  return diff == 0;
}

namespace {
int hexVal(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}
}  // namespace

std::string urlDecode(std::string_view s) {
  std::string out;
  out.reserve(s.size());
  for (size_t i = 0; i < s.size(); ++i) {
    char c = s[i];
    if (c == '+') {
      out += ' ';
    } else if (c == '%' && i + 2 < s.size() &&
               hexVal(s[i + 1]) >= 0 && hexVal(s[i + 2]) >= 0) {
      out += static_cast<char>(hexVal(s[i + 1]) * 16 + hexVal(s[i + 2]));
      i += 2;
    } else {
      out += c;
    }
  }
  return out;
}

}  // namespace rc
