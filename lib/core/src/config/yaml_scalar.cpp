#include "config/yaml_scalar.h"

#include <cstdio>

#include "util/utf8.h"

namespace rc {

std::string yamlQuote(std::string_view s) {
  std::string o = "\"";
  for (char ch : s) {
    unsigned char c = static_cast<unsigned char>(ch);
    switch (c) {
      case '\\': o += "\\\\"; break;
      case '"': o += "\\\""; break;
      case '\n': o += "\\n"; break;
      case '\t': o += "\\t"; break;
      default:
        if (c < 0x20 || c == 0x7F) {
          char b[8];
          snprintf(b, sizeof b, "\\u%04X", c);
          o += b;
        } else {
          o += ch;
        }
    }
  }
  return o + "\"";
}

namespace {
// Only spaces, or a " # comment", may follow a closing quote.
bool onlyTrailer(std::string_view t) {
  size_t i = 0;
  while (i < t.size() && t[i] == ' ') i++;
  if (i == t.size()) return true;
  return t[i] == '#' && i > 0;
}

int hexVal(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}
}  // namespace

bool yamlScalar(std::string_view rest, std::string& out, ScalarKind& kind, std::string& err) {
  out.clear();
  size_t i = 0;
  while (i < rest.size() && rest[i] == ' ') i++;
  rest.remove_prefix(i);

  if (!rest.empty() && rest[0] == '"') {
    kind = ScalarKind::Double;
    size_t p = 1;
    while (p < rest.size()) {
      char c = rest[p];
      if (c == '"') {
        if (!onlyTrailer(rest.substr(p + 1))) {
          err = "unexpected text after the closing quote";
          return false;
        }
        return true;
      }
      if (c != '\\') {
        out += c;
        p++;
        continue;
      }
      if (p + 1 >= rest.size()) break;
      char e = rest[p + 1];
      p += 2;
      switch (e) {
        case '\\': out += '\\'; break;
        case '"': out += '"'; break;
        case 'n': out += '\n'; break;
        case 't': out += '\t'; break;
        case 'r': out += '\r'; break;
        case 'u': {
          if (p + 4 > rest.size()) {
            err = "bad \\u escape";
            return false;
          }
          char32_t cp = 0;
          for (int k = 0; k < 4; k++) {
            int h = hexVal(rest[p + k]);
            if (h < 0) {
              err = "bad \\u escape";
              return false;
            }
            cp = cp * 16 + h;
          }
          p += 4;
          if (cp >= 0xD800 && cp <= 0xDFFF) {
            err = "surrogate in \\u escape";
            return false;
          }
          out += u32ToUtf8(std::u32string_view(&cp, 1));
          break;
        }
        default:
          err = std::string("unknown escape \\") + e;
          return false;
      }
    }
    err = "unclosed double quote";
    return false;
  }

  if (!rest.empty() && rest[0] == '\'') {
    kind = ScalarKind::Single;
    size_t p = 1;
    while (p < rest.size()) {
      char c = rest[p];
      if (c == '\'') {
        if (p + 1 < rest.size() && rest[p + 1] == '\'') {
          out += '\'';
          p += 2;
          continue;
        }
        if (!onlyTrailer(rest.substr(p + 1))) {
          err = "unexpected text after the closing quote";
          return false;
        }
        return true;
      }
      out += c;
      p++;
    }
    err = "unclosed single quote";
    return false;
  }

  kind = ScalarKind::Plain;
  size_t end = rest.size();
  for (size_t p = 0; p + 1 < rest.size(); p++) {
    if (rest[p] == ' ' && rest[p + 1] == '#') {
      end = p;
      break;
    }
  }
  std::string_view v = rest.substr(0, end);
  while (!v.empty() && v.back() == ' ') v.remove_suffix(1);
  out.assign(v);
  return true;
}

}  // namespace rc
