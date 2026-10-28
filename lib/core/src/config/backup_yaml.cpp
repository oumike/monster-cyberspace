#include "config/backup_yaml.h"

#include <algorithm>
#include <ctime>
#include <set>
#include <vector>

#include "config/yaml_scalar.h"
#include "tz/zones.h"
#include "ui/rotation.h"
#include "ui/theme.h"
#include "web/web_util.h"

namespace rc {

namespace {
constexpr const char* kFormat = "monster-cyberspace-backup";
// Written under the project's earlier names; still read.
constexpr const char* kOldFormat = "platy-cyberspace-backup";
constexpr const char* kOlderFormat = "rhino-cyberspace-backup";
constexpr int kVersion = 1;
constexpr int64_t kMinClock = 1700000000;

std::string created(int64_t epochSec) {
  if (epochSec < kMinClock) return "";
  time_t t = static_cast<time_t>(epochSec);
  struct tm tm;
  gmtime_r(&t, &tm);
  char b[24];
  strftime(b, sizeof b, "%Y-%m-%dT%H:%M:%SZ", &tm);
  return b;
}

const char* boolStr(bool b) { return b ? "true" : "false"; }

struct Line {
  int no;
  size_t indent;
  bool tab;  // a tab in the indent
  std::string_view text;
};

struct Str {
  bool has = false;
  int line = 0;
  std::string v;
  ScalarKind kind = ScalarKind::Plain;
};
struct Bool {
  bool has = false;
  bool v = false;
};

struct Item {
  int dashLine = 0;
  Str ssid, password;
};

struct Staged {
  Str format, version, timezone, tzName, ntpServer, pagePassword, apPassword, textSize, theme,
      rotation, lastRoom, email, refreshToken;
  Bool drawerOpen, buzzer, autoUpdate;
  bool hasWifi = false;
  std::vector<Item> wifi;
};

std::string at(int line, const std::string& msg) { return "line " + std::to_string(line) + ": " + msg; }

bool printable(std::string_view s) {
  for (char c : s)
    if (c < 0x20 || c > 0x7E) return false;
  return true;
}

bool splitKey(std::string_view text, std::string_view& key, std::string_view& rest) {
  size_t c = text.find(':');
  if (c == std::string_view::npos || c == 0) return false;
  if (c + 1 < text.size() && text[c + 1] != ' ') return false;
  if (text[0] == '-' && (text.size() == 1 || text[1] == ' ')) return false;
  key = text.substr(0, c);
  while (!key.empty() && key.back() == ' ') key.remove_suffix(1);
  if (key.empty()) return false;
  rest = text.substr(c + 1);
  return true;
}

// The value after "key:". Empty or comment-only gives "" with empty = true.
bool readValue(std::string_view rest, int line, Str& out, bool& empty, std::string& err) {
  size_t i = 0;
  while (i < rest.size() && rest[i] == ' ') i++;
  rest.remove_prefix(i);
  out.has = true;
  out.line = line;
  empty = rest.empty() || rest[0] == '#';
  if (empty) {
    out.v.clear();
    out.kind = ScalarKind::Plain;
    return true;
  }
  std::string e;
  if (!yamlScalar(rest, out.v, out.kind, e)) {
    err = at(line, e);
    return false;
  }
  if (out.v.find('\0') != std::string::npos) {
    err = at(line, "value contains a NUL");
    return false;
  }
  return true;
}

bool readBool(std::string_view rest, int line, Bool& out, std::string& err) {
  Str s;
  bool empty;
  if (!readValue(rest, line, s, empty, err)) return false;
  if (empty || s.kind != ScalarKind::Plain || (s.v != "true" && s.v != "false")) {
    err = at(line, "expected true or false");
    return false;
  }
  out.has = true;
  out.v = s.v == "true";
  return true;
}

bool parseWifi(const std::vector<Line>& ls, size_t from, size_t to, std::vector<Item>& items,
               std::string& err) {
  size_t itemIndent = 0;
  for (size_t k = from; k < to; k++) {
    const Line& l = ls[k];
    if (l.tab) {
      err = at(l.no, "bad indent");
      return false;
    }
    std::string_view t = l.text;
    bool dash = t[0] == '-' && (t.size() == 1 || t[1] == ' ');
    if (dash) {
      if (items.empty()) {
        itemIndent = l.indent;
      } else if (l.indent != itemIndent) {
        err = at(l.no, "bad indent");
        return false;
      }
      if (t.size() < 3 || t[2] == ' ') {
        err = at(l.no, t.size() < 3 ? "expected \"key: value\"" : "bad indent");
        return false;
      }
      t.remove_prefix(2);
      items.emplace_back();
      items.back().dashLine = l.no;
    } else if (items.empty() || l.indent != itemIndent + 2) {
      err = at(l.no, "bad indent");
      return false;
    }
    std::string_view key, rest;
    if (!splitKey(t, key, rest)) {
      err = at(l.no, "expected \"key: value\"");
      return false;
    }
    Item& it = items.back();
    Str* dst = key == "ssid" ? &it.ssid : key == "password" ? &it.password : nullptr;
    Str ignored;
    if (!dst) dst = &ignored;
    if (dst != &ignored && dst->has) {
      err = at(l.no, "duplicate key \"" + std::string(key) + "\"");
      return false;
    }
    bool empty;
    if (!readValue(rest, l.no, *dst, empty, err)) return false;
  }
  return true;
}

bool parseAccount(const std::vector<Line>& ls, size_t from, size_t to, Staged& st,
                  std::string& err) {
  std::set<std::string> seen;
  size_t indent = ls[from].indent;
  for (size_t k = from; k < to; k++) {
    const Line& l = ls[k];
    if (l.tab || l.indent != indent) {
      err = at(l.no, "bad indent");
      return false;
    }
    std::string_view key, rest;
    if (!splitKey(l.text, key, rest)) {
      err = at(l.no, "expected \"key: value\"");
      return false;
    }
    if (!seen.insert(std::string(key)).second) {
      err = at(l.no, "duplicate key \"" + std::string(key) + "\"");
      return false;
    }
    Str ignored;
    Str* dst = key == "email" ? &st.email : key == "refreshToken" ? &st.refreshToken : &ignored;
    bool empty;
    if (!readValue(rest, l.no, *dst, empty, err)) return false;
  }
  return true;
}

bool parse(std::string_view yaml, Staged& st, std::string& err) {
  if (yaml.size() >= 3 && yaml.substr(0, 3) == "\xEF\xBB\xBF") yaml.remove_prefix(3);
  std::vector<Line> ls;
  int no = 0;
  while (!yaml.empty()) {
    size_t nl = yaml.find('\n');
    std::string_view raw = yaml.substr(0, nl);
    yaml = nl == std::string_view::npos ? std::string_view{} : yaml.substr(nl + 1);
    no++;
    if (!raw.empty() && raw.back() == '\r') raw.remove_suffix(1);
    Line l{no, 0, false, {}};
    while (l.indent < raw.size() && (raw[l.indent] == ' ' || raw[l.indent] == '\t')) {
      if (raw[l.indent] == '\t') l.tab = true;
      l.indent++;
    }
    l.text = raw.substr(l.indent);
    if (l.text.empty() || l.text[0] == '#') continue;
    ls.push_back(l);
  }

  std::set<std::string> seen;
  for (size_t i = 0; i < ls.size();) {
    const Line& l = ls[i];
    if (l.tab || l.indent > 0) {
      err = at(l.no, "bad indent");
      return false;
    }
    std::string_view key, rest;
    if (!splitKey(l.text, key, rest)) {
      err = at(l.no, "expected \"key: value\"");
      return false;
    }
    if (!seen.insert(std::string(key)).second) {
      err = at(l.no, "duplicate key \"" + std::string(key) + "\"");
      return false;
    }
    size_t b = i + 1, e = b;
    while (e < ls.size() && (ls[e].indent > 0 || ls[e].tab)) e++;
    i = e;

    Str* s = key == "format"         ? &st.format
             : key == "version"      ? &st.version
             : key == "timezone"     ? &st.timezone
             : key == "tzName"       ? &st.tzName
             : key == "ntpServer"    ? &st.ntpServer
             : key == "pagePassword" ? &st.pagePassword
             : key == "apPassword"   ? &st.apPassword
             : key == "textSize"     ? &st.textSize
             : key == "theme"        ? &st.theme
             : key == "rotation"     ? &st.rotation
             : key == "lastRoom"     ? &st.lastRoom
                                     : nullptr;
    Str ignored;
    bool isBool = key == "drawerOpen" || key == "buzzer" || key == "autoUpdate";
    bool known = s || isBool;
    if (key == "board" || key == "firmware" || key == "created") {
      s = &ignored;
      known = true;
    }
    if (known) {
      if (e > b) {
        err = at(ls[b].no, "bad indent");
        return false;
      }
      if (isBool) {
        Bool& dst = key == "buzzer" ? st.buzzer : key == "autoUpdate" ? st.autoUpdate : st.drawerOpen;
        if (!readBool(rest, l.no, dst, err)) return false;
      } else {
        bool empty;
        if (!readValue(rest, l.no, *s, empty, err)) return false;
      }
    } else if (key == "wifi") {
      Str v;
      bool empty;
      if (!readValue(rest, l.no, v, empty, err)) return false;
      if (empty) {
        if (e > b) {
          st.hasWifi = true;
          if (!parseWifi(ls, b, e, st.wifi, err)) return false;
        }
      } else if (v.kind == ScalarKind::Plain && v.v == "[]" && e == b) {
        st.hasWifi = true;
      } else {
        err = at(l.no, e > b && v.v == "[]" ? "bad indent" : "expected \"key: value\"");
        return false;
      }
    } else if (key == "account") {
      Str v;
      bool empty;
      if (!readValue(rest, l.no, v, empty, err)) return false;
      if (!empty) {
        err = at(l.no, "expected \"key: value\"");
        return false;
      }
      if (e > b && !parseAccount(ls, b, e, st, err)) return false;
    }
  }
  return true;
}

bool positiveInt(const Str& s, int& out) {
  if (!s.has || s.kind != ScalarKind::Plain || s.v.empty() || s.v.size() > 6) return false;
  int n = 0;
  for (char c : s.v) {
    if (c < '0' || c > '9') return false;
    n = n * 10 + (c - '0');
  }
  out = n;
  return n > 0;
}

// A plain integer, optionally negative.
bool plainInt(const Str& s, int& out) {
  if (s.kind != ScalarKind::Plain || s.v.empty() || s.v.size() > 6) return false;
  size_t i = s.v[0] == '-' ? 1 : 0;
  if (i == s.v.size()) return false;
  int n = 0;
  for (; i < s.v.size(); ++i) {
    if (s.v[i] < '0' || s.v[i] > '9') return false;
    n = n * 10 + (s.v[i] - '0');
  }
  out = s.v[0] == '-' ? -n : n;
  return true;
}
}  // namespace

std::string backupToYaml(const Settings& s, const BackupMeta& m) {
  std::string o =
      "# Monster Cyberspace settings backup.\n"
      "# Contains Wi-Fi and device passwords and a Cyberspace sign-in token.\n"
      "# Anyone with this file can use your account: keep it private.\n";
  o += std::string("format: ") + kFormat + "\n";
  o += "version: " + std::to_string(kVersion) + "\n";
  o += "board: " + yamlQuote(m.board) + "\n";
  o += "firmware: " + yamlQuote(m.firmware) + "\n";
  o += "created: " + yamlQuote(created(m.epochSec)) + "\n";
  if (s.wifi.empty()) {
    o += "wifi: []\n";
  } else {
    o += "wifi:\n";
    for (const WifiNet& n : s.wifi) {
      o += "  - ssid: " + yamlQuote(n.ssid) + "\n";
      o += "    password: " + yamlQuote(n.password) + "\n";
    }
  }
  o += "timezone: " + yamlQuote(s.tz) + "\n";
  o += "tzName: " + yamlQuote(s.tzName) + "\n";
  o += "ntpServer: " + yamlQuote(s.ntpServer) + "\n";
  o += "pagePassword: " + yamlQuote(s.pagePassword) + "\n";
  o += "apPassword: " + yamlQuote(s.apPassword) + "\n";
  o += std::string("textSize: ") + textSizeKey(s.textSize) + "\n";
  o += std::string("theme: ") + themeKey(s.theme) + "  # default | bw | wb\n";
  o += "rotation: " + std::to_string(s.rotation) +
       "  # 0 landscape, 1 portrait, 2 landscape 180, 3 portrait 180\n";
  o += std::string("drawerOpen: ") + boolStr(s.drawerOpen) + "\n";
  o += std::string("buzzer: ") + boolStr(s.buzzer) + "\n";
  o += std::string("autoUpdate: ") + boolStr(s.autoUpdate) + "  # install firmware updates on its own\n";
  o += "lastRoom: " + yamlQuote(s.lastRoom) + "\n";
  o += "account:\n";
  o += "  email: " + yamlQuote(s.email) + "\n";
  o += "  refreshToken: " + yamlQuote(s.refreshToken) + "\n";
  return o;
}

bool backupFromYaml(std::string_view yaml, Settings& inOut, std::string& err, std::string& note) {
  err.clear();
  note.clear();
  if (yaml.size() > kMaxBackupBytes) {
    err = "Backup is over 16 KB";
    return false;
  }
  Staged st;
  if (!parse(yaml, st, err)) return false;

  int version = 0;
  if (!st.format.has || (st.format.v != kFormat && st.format.v != kOldFormat && st.format.v != kOlderFormat) || !positiveInt(st.version, version)) {
    err = "Not a Monster Cyberspace backup";
    return false;
  }
  if (version > kVersion) {
    err = "Backup version " + std::to_string(version) + " is newer than this firmware";
    return false;
  }

  std::string wifiNote;
  if (st.wifi.size() > kMaxWifiNets) {
    wifiNote = "Kept the first " + std::to_string(kMaxWifiNets) + " of " +
               std::to_string(st.wifi.size()) + " networks.";
    st.wifi.resize(kMaxWifiNets);
  }
  for (size_t i = 0; i < st.wifi.size(); i++) {
    const Item& it = st.wifi[i];
    if (it.ssid.v.empty() || it.ssid.v.size() > 32) {
      err = at(it.ssid.has ? it.ssid.line : it.dashLine, "network name: 1-32 bytes");
      return false;
    }
    size_t pl = it.password.v.size();
    if (pl != 0 && (pl < 8 || pl > 63)) {
      err = at(it.password.line, "Wi-Fi password: 8-63 characters, or empty");
      return false;
    }
    for (size_t j = 0; j < i; j++) {
      if (st.wifi[j].ssid.v == it.ssid.v) {
        err = at(it.ssid.line, "duplicate network \"" + it.ssid.v + "\"");
        return false;
      }
    }
  }

  ThemeId theme = inOut.theme;
  int rotation = 0;
  if (st.apPassword.has && !validApPassword(st.apPassword.v)) {
    err = at(st.apPassword.line, "setup-AP password: 8-63 printable characters, or empty");
  } else if (st.pagePassword.has &&
             (st.pagePassword.v.size() > 63 || !printable(st.pagePassword.v))) {
    err = at(st.pagePassword.line, "page password: up to 63 printable characters");
  } else if (TextSize ts; st.textSize.has && !parseTextSize(st.textSize.v, ts)) {
    err = at(st.textSize.line, "text size: compact, comfortable or large");
  } else if (st.theme.has && !parseThemeId(st.theme.v, theme)) {
    err = at(st.theme.line, "theme: default, bw or wb");
  } else if (st.rotation.has && !plainInt(st.rotation, rotation)) {
    err = at(st.rotation.line, "orientation: 0 to 3");
  } else if (st.timezone.has && !validPosixTz(st.timezone.v)) {
    err = at(st.timezone.line, "time zone: 1-63 printable characters");
  } else if (st.tzName.has && (st.tzName.v.size() > 63 || !printable(st.tzName.v))) {
    err = at(st.tzName.line, "time zone name: up to 63 printable characters");
  } else if (st.ntpServer.has && !validNtpServer(st.ntpServer.v)) {
    err = at(st.ntpServer.line, "NTP server: a host name, 1-63 characters");
  } else if (st.lastRoom.has && (st.lastRoom.v.empty() || st.lastRoom.v.size() > 63 ||
                                 !printable(st.lastRoom.v))) {
    err = at(st.lastRoom.line, "last room: 1-63 printable characters");
  }
  if (!err.empty()) return false;

  if (st.hasWifi) {
    inOut.wifi.clear();
    for (Item& it : st.wifi) inOut.wifi.push_back({it.ssid.v, it.password.v});
  }
  if (st.timezone.has) {
    inOut.tz = st.timezone.v;
    if (!st.tzName.has) {
      const Zone* z = findZoneByPosix(st.timezone.v);
      inOut.tzName = z ? z->iana : "";
    }
  }
  if (st.tzName.has) inOut.tzName = st.tzName.v;
  if (st.ntpServer.has) inOut.ntpServer = st.ntpServer.v;
  if (st.pagePassword.has) inOut.pagePassword = st.pagePassword.v;
  if (st.apPassword.has) inOut.apPassword = st.apPassword.v;
  if (st.textSize.has)
    parseTextSize(st.textSize.v, inOut.textSize);
  if (st.theme.has) inOut.theme = theme;
  if (st.rotation.has) inOut.rotation = rotationFromIndex(rotation);
  if (st.drawerOpen.has) inOut.drawerOpen = st.drawerOpen.v;
  if (st.buzzer.has) inOut.buzzer = st.buzzer.v;
  if (st.autoUpdate.has) inOut.autoUpdate = st.autoUpdate.v;
  if (st.lastRoom.has) inOut.lastRoom = st.lastRoom.v;
  if (st.email.has) inOut.email = st.email.v;
  if (st.refreshToken.has) inOut.refreshToken = st.refreshToken.v;
  note = wifiNote;
  return true;
}

namespace {
constexpr std::string_view kPrefix = "rc-backup-";
constexpr std::string_view kExt = ".yaml";

bool allDigits(std::string_view v) {
  if (v.empty()) return false;
  for (char c : v)
    if (c < '0' || c > '9') return false;
  return true;
}

// Stem between prefix and ".yaml", or nullopt-like false.
bool stemOf(std::string_view name, std::string_view& stem) {
  if (name.size() <= kPrefix.size() + kExt.size()) return false;
  if (name.substr(0, kPrefix.size()) != kPrefix) return false;
  if (name.substr(name.size() - kExt.size()) != kExt) return false;
  stem = name.substr(kPrefix.size(), name.size() - kPrefix.size() - kExt.size());
  return true;
}

// Dated: "YYYYMMDD-HHMMSS" or "YYYYMMDD-HHMMSS-N". stamp = the 15 chars, seq = N (1 when absent).
bool datedParts(std::string_view name, std::string_view& stamp, long& seq) {
  std::string_view stem;
  if (!stemOf(name, stem) || stem.size() < 15) return false;
  if (!allDigits(stem.substr(0, 8)) || stem[8] != '-' || !allDigits(stem.substr(9, 6))) return false;
  stamp = stem.substr(0, 15);
  seq = 1;
  if (stem.size() == 15) return true;
  if (stem[15] != '-' || !allDigits(stem.substr(16)) || stem.size() > 25) return false;
  seq = std::stol(std::string(stem.substr(16)));
  return true;
}

bool numbered(std::string_view name, long& n) {
  std::string_view stem;
  if (!stemOf(name, stem) || !allDigits(stem) || stem.size() > 9) return false;
  n = std::stol(std::string(stem));
  return true;
}
}  // namespace

std::string backupFileName(int64_t epochSec, const std::vector<std::string>& existing) {
  auto has = [&](const std::string& n) { return std::find(existing.begin(), existing.end(), n) != existing.end(); };
  if (epochSec >= kMinClock) {
    time_t t = static_cast<time_t>(epochSec);
    struct tm tm;
    gmtime_r(&t, &tm);
    char b[16];
    strftime(b, sizeof b, "%Y%m%d-%H%M%S", &tm);
    std::string base = std::string(kPrefix) + b;
    std::string name = base + std::string(kExt);
    for (int n = 2; has(name); ++n) name = base + "-" + std::to_string(n) + std::string(kExt);
    return name;
  }
  long maxN = 0;
  for (const auto& e : existing) {
    long n;
    if (numbered(e, n)) maxN = std::max(maxN, n);
  }
  return std::string(kPrefix) + std::to_string(maxN + 1) + std::string(kExt);
}

bool validBackupName(std::string_view name) {
  if (name.size() <= kExt.size() || name.size() > 64) return false;
  if (name.substr(name.size() - kExt.size()) != kExt) return false;
  for (char c : name) {
    bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' ||
              c == '_' || c == '-';
    if (!ok) return false;
  }
  return true;
}

void sortBackupNames(std::vector<std::string>& names) {
  auto rank = [](const std::string& s, std::string_view& stamp, long& n) {
    if (datedParts(s, stamp, n)) return 0;
    if (numbered(s, n)) return 1;
    return 2;
  };
  std::sort(names.begin(), names.end(), [&](const std::string& a, const std::string& b) {
    std::string_view sa, sb;
    long na = 0, nb = 0;
    int ra = rank(a, sa, na), rb = rank(b, sb, nb);
    if (ra != rb) return ra < rb;
    if (ra == 0) return sa != sb ? sa > sb : na > nb;
    if (ra == 1) return na != nb ? na > nb : a < b;
    return a < b;
  });
}

}  // namespace rc
