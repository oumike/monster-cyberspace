#include "store/settings.h"

#include <Preferences.h>

namespace {

constexpr const char* kNs = "rcfg";

std::string getStr(Preferences& p, const char* key, const std::string& def) {
  if (!p.isKey(key)) return def;
  return p.getString(key, def.c_str()).c_str();
}

// Keys must be at most 15 characters.
void ssidKey(char* buf, size_t n, size_t i) { snprintf(buf, n, "ws%u", unsigned(i)); }
void passKey(char* buf, size_t n, size_t i) { snprintf(buf, n, "wp%u", unsigned(i)); }

}  // namespace

namespace settings {

void load(rc::Settings& s) {
  s = rc::Settings{};
  Preferences p;
  if (!p.begin(kNs, true)) return;  // nothing saved yet
  size_t n = p.getUChar("wn", 0);
  if (n > rc::kMaxWifiNets) n = rc::kMaxWifiNets;
  char k[8];
  for (size_t i = 0; i < n; ++i) {
    rc::WifiNet w;
    ssidKey(k, sizeof k, i);
    w.ssid = getStr(p, k, "");
    passKey(k, sizeof k, i);
    w.password = getStr(p, k, "");
    if (!w.ssid.empty()) s.wifi.push_back(std::move(w));
  }
  s.email = getStr(p, "email", "");
  s.refreshToken = getStr(p, "rtok", "");
  s.tz = getStr(p, "tz", s.tz);
  s.pagePassword = getStr(p, "pagepw", "");
  s.apPassword = getStr(p, "appw", "");
  s.textSize = p.getUChar("tsize", 1) == 0 ? rc::TextSize::Comfortable : rc::TextSize::Compact;
  s.drawerOpen = p.getBool("drawer", false);
  s.buzzer = p.getBool("buzz", true);
  s.lastRoom = getStr(p, "room", s.lastRoom);
  p.end();
}

void save(const rc::Settings& s) {
  Preferences p;
  if (!p.begin(kNs, false)) return;
  size_t n = s.wifi.size() < rc::kMaxWifiNets ? s.wifi.size() : rc::kMaxWifiNets;
  char k[8];
  for (size_t i = 0; i < rc::kMaxWifiNets; ++i) {
    ssidKey(k, sizeof k, i);
    if (i < n) p.putString(k, s.wifi[i].ssid.c_str()); else p.remove(k);
    passKey(k, sizeof k, i);
    if (i < n) p.putString(k, s.wifi[i].password.c_str()); else p.remove(k);
  }
  p.putUChar("wn", uint8_t(n));
  p.putString("email", s.email.c_str());
  p.putString("rtok", s.refreshToken.c_str());
  p.putString("tz", s.tz.c_str());
  p.putString("pagepw", s.pagePassword.c_str());
  p.putString("appw", s.apPassword.c_str());
  p.putUChar("tsize", s.textSize == rc::TextSize::Comfortable ? 0 : 1);
  p.putBool("drawer", s.drawerOpen);
  p.putBool("buzz", s.buzzer);
  p.putString("room", s.lastRoom.c_str());
  p.end();
}

void saveToken(const std::string& refreshToken) {
  Preferences p;
  if (!p.begin(kNs, false)) return;
  p.putString("rtok", refreshToken.c_str());
  p.end();
}

void clearToken() {
  Preferences p;
  if (!p.begin(kNs, false)) return;
  p.remove("rtok");
  p.end();
}

void factoryReset() {
  Preferences p;
  if (!p.begin(kNs, false)) return;
  p.clear();
  p.end();
}

}  // namespace settings
