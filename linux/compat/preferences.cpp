// SPDX-License-Identifier: GPL-3.0-or-later
// The file format, one entry a line: "<type> <key> <value>", type u8, u16,
// u32 or str; a str value escapes backslash, newline and carriage return.
#include <Preferences.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <fstream>
#include <map>
#include <mutex>
#include <sstream>

namespace {

enum class Type { U8, U16, U32, Str };

struct Entry {
  Type type;
  uint32_t num = 0;
  std::string str;
};

using Namespace = std::map<std::string, Entry>;

std::mutex lock;
std::map<std::string, Namespace> spaces;  // the namespaces read or written so far

constexpr size_t kMaxKey = 15;  // NVS's limit

bool keyOk(const char* key) { return key && *key && std::strlen(key) <= kMaxKey; }

bool mkdirs(const std::string& dir) {
  for (size_t i = 1; i <= dir.size(); ++i) {
    if (i == dir.size() || dir[i] == '/') {
      const std::string part = dir.substr(0, i);
      if (::mkdir(part.c_str(), 0700) != 0 && errno != EEXIST) return false;
    }
  }
  return ::chmod(dir.c_str(), 0700) == 0;
}

std::string escape(const std::string& s) {
  std::string o;
  for (char c : s) {
    if (c == '\\') o += "\\\\";
    else if (c == '\n') o += "\\n";
    else if (c == '\r') o += "\\r";
    else o += c;
  }
  return o;
}

std::string unescape(const std::string& s) {
  std::string o;
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '\\' && i + 1 < s.size()) {
      const char c = s[++i];
      o += c == 'n' ? '\n' : c == 'r' ? '\r' : c;
    } else {
      o += s[i];
    }
  }
  return o;
}

std::string pathOf(const std::string& ns) { return compat::configDir() + "/" + ns; }

bool readFile(const std::string& ns, Namespace& out) {
  std::ifstream in(pathOf(ns), std::ios::binary);
  if (!in) return false;
  std::string line;
  while (std::getline(in, line)) {
    const size_t a = line.find(' ');
    const size_t b = a == std::string::npos ? a : line.find(' ', a + 1);
    if (b == std::string::npos) continue;
    const std::string t = line.substr(0, a), key = line.substr(a + 1, b - a - 1), v = line.substr(b + 1);
    Entry e;
    if (t == "str") {
      e.type = Type::Str;
      e.str = unescape(v);
    } else {
      e.type = t == "u8" ? Type::U8 : t == "u16" ? Type::U16 : Type::U32;
      if (t != "u8" && t != "u16" && t != "u32") continue;
      e.num = uint32_t(std::strtoul(v.c_str(), nullptr, 10));
    }
    out[key] = e;
  }
  return true;
}

bool writeFile(const std::string& ns, const Namespace& n) {
  std::ostringstream o;
  for (const auto& [k, e] : n) {
    switch (e.type) {
      case Type::U8: o << "u8 " << k << ' ' << e.num << '\n'; break;
      case Type::U16: o << "u16 " << k << ' ' << e.num << '\n'; break;
      case Type::U32: o << "u32 " << k << ' ' << e.num << '\n'; break;
      case Type::Str: o << "str " << k << ' ' << escape(e.str) << '\n'; break;
    }
  }
  return compat::writeConfigFile(ns, o.str());
}

// The namespace, read from its file on first use; nullptr if it has none.
Namespace* find(const std::string& ns) {
  auto it = spaces.find(ns);
  if (it != spaces.end()) return &it->second;
  Namespace n;
  if (!readFile(ns, n)) return nullptr;
  return &(spaces[ns] = std::move(n));
}

}  // namespace

std::string compat::configDir() {
  if (const char* d = std::getenv("MONSTER_CONFIG_DIR"); d && *d) return d;
  if (const char* x = std::getenv("XDG_CONFIG_HOME"); x && *x) return std::string(x) + "/monster-cyberspace";
  const char* home = std::getenv("HOME");
  return std::string(home && *home ? home : ".") + "/.config/monster-cyberspace";
}

std::string compat::dataDir() {
  if (const char* d = std::getenv("MONSTER_DATA_DIR"); d && *d) return d;
  const char* home = std::getenv("HOME");
  return std::string(home && *home ? home : ".") + "/monster-cyberspace";
}

bool compat::makeDirs(const std::string& dir) { return mkdirs(dir); }

bool compat::writeConfigFile(const std::string& name, const std::string& bytes) {
  if (!mkdirs(compat::configDir())) return false;
  const std::string path = pathOf(name), tmp = path + ".tmp";
  const int fd = ::open(tmp.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
  if (fd < 0) return false;
  bool ok = ::fchmod(fd, 0600) == 0;
  for (size_t done = 0; ok && done < bytes.size();) {
    const ssize_t w = ::write(fd, bytes.data() + done, bytes.size() - done);
    if (w <= 0) ok = false;
    else done += size_t(w);
  }
  ok = ::fsync(fd) == 0 && ok;
  ok = ::close(fd) == 0 && ok;
  if (ok) ok = ::rename(tmp.c_str(), path.c_str()) == 0;
  if (!ok) ::unlink(tmp.c_str());
  return ok;
}

void compat::preferencesForget() {
  std::lock_guard<std::mutex> g(lock);
  spaces.clear();
}

bool Preferences::begin(const char* name, bool readOnly) {
  end();
  if (!keyOk(name)) return false;
  std::lock_guard<std::mutex> g(lock);
  if (!find(name)) {
    if (readOnly) return false;
    if (!writeFile(name, {})) return false;
    spaces[name] = {};
  }
  ns_ = name;
  readOnly_ = readOnly;
  open_ = true;
  return true;
}

void Preferences::end() { open_ = false; }

bool Preferences::clear() {
  if (!open_ || readOnly_) return false;
  std::lock_guard<std::mutex> g(lock);
  Namespace* n = find(ns_);
  if (!n) return false;
  Namespace keep = *n;
  n->clear();
  if (writeFile(ns_, *n)) return true;
  *n = std::move(keep);
  return false;
}

bool Preferences::remove(const char* key) {
  if (!open_ || readOnly_ || !keyOk(key)) return false;
  std::lock_guard<std::mutex> g(lock);
  Namespace* n = find(ns_);
  if (!n) return false;
  auto it = n->find(key);
  if (it == n->end()) return false;
  const Entry old = it->second;
  n->erase(it);
  if (writeFile(ns_, *n)) return true;
  (*n)[key] = old;
  return false;
}

bool Preferences::isKey(const char* key) {
  if (!open_ || !keyOk(key)) return false;
  std::lock_guard<std::mutex> g(lock);
  Namespace* n = find(ns_);
  return n && n->count(key);
}

namespace {

size_t put(bool open, bool readOnly, const std::string& ns, const char* key, const Entry& e, size_t bytes) {
  if (!open || readOnly || !keyOk(key)) return 0;
  std::lock_guard<std::mutex> g(lock);
  Namespace* n = find(ns);
  if (!n) return 0;
  auto it = n->find(key);
  const bool had = it != n->end();
  const Entry old = had ? it->second : Entry{};
  (*n)[key] = e;
  if (writeFile(ns, *n)) return bytes;
  if (had) (*n)[key] = old;
  else n->erase(key);
  return 0;
}

bool get(bool open, const std::string& ns, const char* key, Type type, Entry& out) {
  if (!open || !keyOk(key)) return false;
  std::lock_guard<std::mutex> g(lock);
  Namespace* n = find(ns);
  if (!n) return false;
  auto it = n->find(key);
  if (it == n->end() || it->second.type != type) return false;
  out = it->second;
  return true;
}

}  // namespace

size_t Preferences::putBool(const char* key, bool v) { return putUChar(key, v ? 1 : 0); }
size_t Preferences::putUChar(const char* key, uint8_t v) {
  return put(open_, readOnly_, ns_, key, Entry{Type::U8, v, {}}, 1);
}
size_t Preferences::putUShort(const char* key, uint16_t v) {
  return put(open_, readOnly_, ns_, key, Entry{Type::U16, v, {}}, 2);
}
size_t Preferences::putUInt(const char* key, uint32_t v) {
  return put(open_, readOnly_, ns_, key, Entry{Type::U32, v, {}}, 4);
}
size_t Preferences::putString(const char* key, const char* v) {
  if (!v) return 0;
  return put(open_, readOnly_, ns_, key, Entry{Type::Str, 0, v}, std::strlen(v));
}

bool Preferences::getBool(const char* key, bool def) { return getUChar(key, def ? 1 : 0) != 0; }

uint8_t Preferences::getUChar(const char* key, uint8_t def) {
  Entry e;
  return get(open_, ns_, key, Type::U8, e) ? uint8_t(e.num) : def;
}

uint16_t Preferences::getUShort(const char* key, uint16_t def) {
  Entry e;
  return get(open_, ns_, key, Type::U16, e) ? uint16_t(e.num) : def;
}

uint32_t Preferences::getUInt(const char* key, uint32_t def) {
  Entry e;
  return get(open_, ns_, key, Type::U32, e) ? e.num : def;
}

String Preferences::getString(const char* key, const String& def) {
  Entry e;
  return get(open_, ns_, key, Type::Str, e) ? String(e.str) : def;
}
