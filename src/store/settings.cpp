#include "store/settings.h"

#include <Preferences.h>

#include "hal/board.h"

namespace {

constexpr const char* kNs = "rcfg";

std::string getStr(Preferences& p, const char* key, const std::string& def) {
  if (!p.isKey(key)) return def;
  return p.getString(key, def.c_str()).c_str();
}

// NVS "tzmode": 0 Automatic, 1 Manual. Unset: Automatic with a GPS. A board
// without a GPS is always Manual.
rc::TzMode loadTzMode(Preferences* p) {
  if (!board::hasGps()) return rc::TzMode::Manual;
  if (!p || !p->isKey("tzmode")) return rc::TzMode::Automatic;
  return p->getUChar("tzmode", 0) == 1 ? rc::TzMode::Manual : rc::TzMode::Automatic;
}

// Keys must be at most 15 characters.
void ssidKey(char* buf, size_t n, size_t i) { snprintf(buf, n, "ws%u", unsigned(i)); }
void passKey(char* buf, size_t n, size_t i) { snprintf(buf, n, "wp%u", unsigned(i)); }

}  // namespace

namespace settings {

void load(rc::Settings& s) {
  s = rc::Settings{};
  Preferences p;
  if (!p.begin(kNs, true)) {  // nothing saved yet
    s.tzMode = loadTzMode(nullptr);
    return;
  }
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
  s.tzMode = loadTzMode(&p);
  s.tzAutoName = getStr(p, "tzaname", "");
  s.tzAutoPosix = getStr(p, "tzaposix", "");
  s.ntpServer = getStr(p, "ntp", s.ntpServer);
  s.pagePassword = getStr(p, "pagepw", "");
  s.apPassword = getStr(p, "appw", "");
  s.textSize = p.getUChar("tsize", 1) == 0 ? rc::TextSize::Comfortable : rc::TextSize::Compact;
  s.drawerOpen = p.getBool("drawer", false);
  s.buzzer = p.getBool("buzz", true);
  s.theme = rc::themeFromIndex(p.getUChar("theme", 0));
  s.lastRoom = getStr(p, "room", s.lastRoom);
  p.end();
}

void saveUiPrefs(const rc::Settings& s) {
  Preferences p;
  if (!p.begin(kNs, false)) return;
  p.putUChar("tsize", s.textSize == rc::TextSize::Comfortable ? 0 : 1);
  p.putBool("drawer", s.drawerOpen);
  p.putBool("buzz", s.buzzer);
  p.putUChar("theme", static_cast<uint8_t>(s.theme));
  p.putString("room", s.lastRoom.c_str());
  p.end();
}

void saveWifi(const std::vector<rc::WifiNet>& nets) {
  Preferences p;
  if (!p.begin(kNs, false)) return;
  size_t n = nets.size() < rc::kMaxWifiNets ? nets.size() : rc::kMaxWifiNets;
  char k[8];
  for (size_t i = 0; i < rc::kMaxWifiNets; ++i) {
    ssidKey(k, sizeof k, i);
    if (i < n) p.putString(k, nets[i].ssid.c_str()); else if (p.isKey(k)) p.remove(k);
    passKey(k, sizeof k, i);
    if (i < n) p.putString(k, nets[i].password.c_str()); else if (p.isKey(k)) p.remove(k);
  }
  p.putUChar("wn", uint8_t(n));
  p.end();
}

void saveTz(const std::string& tz) {
  Preferences p;
  if (!p.begin(kNs, false)) return;
  p.putString("tz", tz.c_str());
  p.end();
}

void saveTzMode(rc::TzMode mode) {
  Preferences p;
  if (!p.begin(kNs, false)) return;
  p.putUChar("tzmode", mode == rc::TzMode::Manual ? 1 : 0);
  p.end();
}

void saveNtpServer(const std::string& host) {
  Preferences p;
  if (!p.begin(kNs, false)) return;
  p.putString("ntp", host.c_str());
  p.end();
}

void saveTzAuto(const std::string& name, const std::string& posix) {
  Preferences p;
  if (!p.begin(kNs, false)) return;
  p.putString("tzaname", name.c_str());
  p.putString("tzaposix", posix.c_str());
  p.end();
}

void saveSecurity(const std::string& pagePassword, const std::string& apPassword) {
  Preferences p;
  if (!p.begin(kNs, false)) return;
  p.putString("pagepw", pagePassword.c_str());
  p.putString("appw", apPassword.c_str());
  p.end();
}

void saveToken(const std::string& refreshToken) {
  Preferences p;
  if (!p.begin(kNs, false)) return;
  p.putString("rtok", refreshToken.c_str());
  p.end();
}

void saveAccount(const std::string& email, const std::string& refreshToken) {
  Preferences p;
  if (!p.begin(kNs, false)) return;
  p.putString("email", email.c_str());
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
