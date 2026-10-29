#include "net/wifi_mgr.h"

#include <Arduino.h>
#include <ESPmDNS.h>
#include <WiFi.h>

#include <algorithm>

#include "app/time_service.h"
#include "hal/board.h"
#include "net/ap_fallback.h"
#include "web/web_util.h"

namespace wifimgr {

namespace {

constexpr const char* kHost = "monster-cyberspace";
constexpr uint32_t kAttemptMs = 12000;  // per network before trying the next
constexpr uint32_t kMaxBackoffMs = 60000;

Mode gMode = Mode::Station;
std::vector<rc::WifiNet> gNets;
std::string gApSsid;
bool gApPassword = false;
std::string gApPw;  // the configured setup-AP password; never logged
bool gMdns = false;
rc::ApFallback gFallback;

size_t gNext = 0;          // index of the next network to try
bool gTrying = false;
bool gWasConnected = false;  // connected at the last reconnectWithBackoff()
uint32_t gAttemptStart = 0;
uint32_t gBackoffMs = 1000;
uint32_t gWaitUntil = 0;   // millis() before which no new pass starts
bool gBootSetup = false;   // setup mode because nothing was saved
std::string gWatch;        // the network the device asked to join now
int gJoinResult = 0;

void startMdns() {
  if (gMdns) return;
  if (!MDNS.begin(kHost)) {
    Serial.println("[wifi] mDNS failed");
    return;
  }
  MDNS.addService("http", "tcp", 80);
  gMdns = true;
}

void raiseAp() {
  bool usePw = !gApPw.empty();
  if (usePw && !rc::validApPassword(gApPw)) {
    Serial.println("[wifi] setup-AP password invalid, AP stays open");
    usePw = false;
  }
  gApPassword = usePw;
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1),
                    IPAddress(255, 255, 255, 0));
  if (!WiFi.softAP(gApSsid.c_str(), usePw ? gApPw.c_str() : nullptr))
    Serial.println("[wifi] softAP failed");
}

void apOn() {
  if (gMode == Mode::Setup) return;
  WiFi.mode(WIFI_AP_STA);
  raiseAp();
  gMode = Mode::Setup;
  Serial.println("[wifi] setup AP up");
}

void apOff() {
  if (gMode != Mode::Setup || gNets.empty()) return;
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  gMode = Mode::Station;
  Serial.println("[wifi] setup AP down");
}

void tryNext() {
  if (gNets.empty()) return;
  const rc::WifiNet& n = gNets[gNext];
  Serial.printf("[wifi] joining \"%s\"\n", n.ssid.c_str());
  WiFi.disconnect(false, false);
  WiFi.begin(n.ssid.c_str(), n.password.empty() ? nullptr : n.password.c_str());
  gTrying = true;
  gAttemptStart = millis();
}

}  // namespace

void begin(const rc::Settings& s) {
  gNets.assign(s.wifi.begin(),
               s.wifi.begin() + std::min(s.wifi.size(), rc::kMaxWifiNets));
  gMode = gNets.empty() ? Mode::Setup : Mode::Station;
  gBootSetup = gNets.empty();
  gApSsid = rc::setupApSsid(board::name());

  WiFi.persistent(false);  // settings live in our own NVS namespace
  WiFi.setHostname(kHost);
  WiFi.setAutoReconnect(false);  // reconnectWithBackoff() owns retries
  WiFi.mode(gMode == Mode::Setup ? WIFI_AP_STA : WIFI_STA);

  gApPw = s.apPassword;
  if (gMode == Mode::Setup) raiseAp();

  startMdns();

  gNext = 0;
  gBackoffMs = 1000;
  gWaitUntil = 0;
  gTrying = false;
  gWasConnected = false;
  if (!gNets.empty()) tryNext();
}

Mode mode() { return gMode; }

bool connected() { return WiFi.status() == WL_CONNECTED; }

int rssiBars() {
  if (!connected()) return 0;
  return rc::wifiBarsFor(WiFi.RSSI());
}

std::string ssid() { return connected() ? std::string(WiFi.SSID().c_str()) : std::string(); }

std::string ip() {
  return connected() ? std::string(WiFi.localIP().toString().c_str()) : std::string();
}

std::string apSsid() { return gApSsid; }

bool apHasPassword() { return gApPassword; }

void setSetupAp(bool on) {
  if (on) apOn();
  else apOff();
  gFallback.noteManual(millis(), mode() == Mode::Setup);
}

std::vector<rc::WifiScanEntry> scanDetailed() {
  std::vector<rc::WifiScanEntry> found;
  int n = WiFi.scanNetworks(false, false);
  for (int i = 0; i < n; ++i) {
    std::string name = WiFi.SSID(i).c_str();
    if (name.empty()) continue;
    const int rssi = WiFi.RSSI(i);
    auto it = std::find_if(found.begin(), found.end(),
                           [&](const rc::WifiScanEntry& f) { return f.ssid == name; });
    if (it == found.end())
      found.push_back(rc::WifiScanEntry{name, rssi, WiFi.encryptionType(i) != WIFI_AUTH_OPEN});
    else if (rssi > it->rssi) it->rssi = rssi;
  }
  WiFi.scanDelete();
  std::sort(found.begin(), found.end(),
            [](const rc::WifiScanEntry& a, const rc::WifiScanEntry& b) { return a.rssi > b.rssi; });
  return found;
}

std::vector<std::string> scan() {
  std::vector<std::string> out;
  for (auto& f : scanDetailed()) out.push_back(std::move(f.ssid));
  return out;
}

void setNetworks(const std::vector<rc::WifiNet>& nets, const std::string& joinNow) {
  const std::string current = ssid();
  gNets.assign(nets.begin(), nets.begin() + std::min(nets.size(), rc::kMaxWifiNets));
  gTrying = false;
  gNext = 0;
  gBackoffMs = 1000;
  gWaitUntil = millis();
  if (gNets.empty()) {
    gWatch.clear();
    WiFi.disconnect(false, false);
    apOn();
    gBootSetup = true;
    return;
  }
  if (!joinNow.empty()) {
    for (size_t i = 0; i < gNets.size(); ++i)
      if (gNets[i].ssid == joinNow) gNext = i;
    gWatch = joinNow;
    gJoinResult = 0;
    tryNext();
    return;
  }
  const bool kept = std::any_of(gNets.begin(), gNets.end(),
                                [&](const rc::WifiNet& n) { return n.ssid == current; });
  if (!current.empty() && !kept) WiFi.disconnect(false, false);
}

int takeJoinResult() {
  const int r = gJoinResult;
  gJoinResult = 0;
  return r;
}

void reconnectWithBackoff() {
  if (gNets.empty()) return;
  switch (gFallback.update(millis(), connected(), gMode == Mode::Setup,
                           gMode == Mode::Setup ? WiFi.softAPgetStationNum() : 0)) {
    case rc::ApFallback::Act::Raise:
      Serial.println("[wifi] saved networks unreachable for 5 min");
      apOn();
      break;
    case rc::ApFallback::Act::Drop:
      apOff();
      break;
    default:
      break;
  }
  const bool up = connected();
  // Every join restarts SNTP, including one that lands after its attempt timed out.
  if (up && !gWasConnected) {
    Serial.printf("[wifi] connected, ip %s\n", WiFi.localIP().toString().c_str());
    rc::timesvc::restartNtp();
  }
  gWasConnected = up;
  if (up && !gWatch.empty() && ssid() == gWatch) {
    gWatch.clear();
    gJoinResult = 1;
    // The AP was only up because nothing was saved: it has done its job.
    if (gBootSetup && gMode == Mode::Setup && WiFi.softAPgetStationNum() == 0) apOff();
    gBootSetup = false;
  }
  if (up) {
    gTrying = false;
    gBackoffMs = 1000;
    gNext = 0;
    gWaitUntil = millis();  // a later drop retries at once
    return;
  }
  uint32_t now = millis();
  if (gTrying) {
    if (now - gAttemptStart < kAttemptMs) return;
    gTrying = false;
    if (!gWatch.empty() && gNets[gNext].ssid == gWatch) {
      gWatch.clear();
      gJoinResult = -1;
    }
    if (++gNext < gNets.size()) {
      tryNext();
      return;
    }
    // A full pass failed: wait, then start again from the first network.
    gNext = 0;
    gWaitUntil = now + gBackoffMs;
    Serial.printf("[wifi] no network joined, retry in %us\n", unsigned(gBackoffMs / 1000));
    gBackoffMs = std::min(gBackoffMs * 2, kMaxBackoffMs);
    return;
  }
  if ((int32_t)(now - gWaitUntil) >= 0) tryNext();
}

}  // namespace wifimgr
