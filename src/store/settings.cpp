#include "store/settings.h"

#include <Preferences.h>

#include "tz/zones.h"
#include "ui/rotation.h"

namespace {

constexpr const char* kNs = "rcfg";

std::string getStr(Preferences& p, const char* key, const std::string& def) {
  if (!p.isKey(key)) return def;
  return p.getString(key, def.c_str()).c_str();
}

// Left by the automatic time zone, which is gone; erased on the first load.
constexpr const char* kStaleKeys[] = {"tzmode", "tzaname", "tzaposix"};

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
  if (p.isKey("tzname")) {
    s.tzName = getStr(p, "tzname", "");
  } else {  // saved before zone names: name it from the list when it matches
    const rc::Zone* z = rc::findZoneByPosix(s.tz);
    s.tzName = z ? z->iana : "";
  }
  s.ntpServer = getStr(p, "ntp", s.ntpServer);
  s.pagePassword = getStr(p, "pagepw", "");
  s.apPassword = getStr(p, "appw", "");
  s.textSize = rc::textSizeFromIndex(p.getUChar("tsize", 1));
  s.drawerOpen = p.getBool("drawer", false);
  s.buzzer = p.getBool("buzz", true);
  s.autoUpdate = p.getBool("otaauto", false);
  s.theme = rc::themeFromIndex(p.getUChar("theme", 0));
  s.rotation = rc::rotationFromIndex(p.getUChar("rot", 0));
  s.lastRoom = getStr(p, "room", s.lastRoom);
  p.end();
}

void saveUiPrefs(const rc::Settings& s) {
  Preferences p;
  if (!p.begin(kNs, false)) return;
  p.putUChar("tsize", static_cast<uint8_t>(s.textSize));
  p.putBool("drawer", s.drawerOpen);
  p.putBool("buzz", s.buzzer);
  p.putUChar("theme", static_cast<uint8_t>(s.theme));
  p.putUChar("rot", s.rotation);
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

void saveTz(const std::string& tz, const std::string& tzName) {
  Preferences p;
  if (!p.begin(kNs, false)) return;
  p.putString("tz", tz.c_str());
  p.putString("tzname", tzName.c_str());
  // Left over from the automatic zone; unread, cleared on the next zone save.
  for (const char* k : kStaleKeys)
    if (p.isKey(k)) p.remove(k);
  p.end();
}

void saveNtpServer(const std::string& host) {
  Preferences p;
  if (!p.begin(kNs, false)) return;
  p.putString("ntp", host.c_str());
  p.end();
}

void saveSecurity(const std::string& pagePassword, const std::string& apPassword) {
  Preferences p;
  if (!p.begin(kNs, false)) return;
  p.putString("pagepw", pagePassword.c_str());
  p.putString("appw", apPassword.c_str());
  p.end();
}

void saveAutoUpdate(bool on) {
  Preferences p;
  if (!p.begin(kNs, false)) return;
  p.putBool("otaauto", on);
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

void saveAll(const rc::Settings& s) {
  saveWifi(s.wifi);
  saveTz(s.tz, s.tzName);
  saveNtpServer(s.ntpServer);
  saveSecurity(s.pagePassword, s.apPassword);
  saveUiPrefs(s);
  saveAutoUpdate(s.autoUpdate);
  saveAccount(s.email, s.refreshToken);
  if (s.refreshToken.empty()) clearToken();
}

void factoryReset() {
  Preferences p;
  if (!p.begin(kNs, false)) return;
  p.clear();
  p.end();
}

}  // namespace settings
