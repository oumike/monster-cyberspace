#include "net/wifi_mgr.h"

#include <Arduino.h>
#include <ESPmDNS.h>
#include <WiFi.h>

#include <algorithm>

#include "hal/board.h"
#include "web/web_util.h"

namespace wifimgr {

namespace {

constexpr const char* kHost = "rhino-cyberspace";
constexpr uint32_t kAttemptMs = 12000;  // per network before trying the next
constexpr uint32_t kMaxBackoffMs = 60000;

Mode gMode = Mode::Station;
std::vector<rc::WifiNet> gNets;
std::string gApSsid;
bool gApPassword = false;
bool gMdns = false;

size_t gNext = 0;          // index of the next network to try
bool gTrying = false;
uint32_t gAttemptStart = 0;
uint32_t gBackoffMs = 1000;
uint32_t gWaitUntil = 0;   // millis() before which no new pass starts

void startMdns() {
  if (gMdns) return;
  if (!MDNS.begin(kHost)) {
    Serial.println("[wifi] mDNS failed");
    return;
  }
  MDNS.addService("http", "tcp", 80);
  gMdns = true;
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
  gApSsid = rc::setupApSsid(board::name());

  WiFi.persistent(false);  // settings live in our own NVS namespace
  WiFi.setHostname(kHost);
  WiFi.setAutoReconnect(false);  // reconnectWithBackoff() owns retries
  WiFi.mode(gMode == Mode::Setup ? WIFI_AP_STA : WIFI_STA);

  if (gMode == Mode::Setup) {
    bool usePw = !s.apPassword.empty();
    if (usePw && !rc::validApPassword(s.apPassword)) {
      Serial.println("[wifi] setup-AP password invalid, AP stays open");
      usePw = false;
    }
    gApPassword = usePw;
    WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1),
                      IPAddress(255, 255, 255, 0));
    if (!WiFi.softAP(gApSsid.c_str(), usePw ? s.apPassword.c_str() : nullptr))
      Serial.println("[wifi] softAP failed");
  }

  configTzTime(s.tz.c_str(), "pool.ntp.org", "time.google.com");
  startMdns();

  gNext = 0;
  gBackoffMs = 1000;
  gWaitUntil = 0;
  gTrying = false;
  if (!gNets.empty()) tryNext();
}

Mode mode() { return gMode; }

bool connected() { return WiFi.status() == WL_CONNECTED; }

int rssiBars() {
  if (!connected()) return 0;
  int r = WiFi.RSSI();
  if (r >= -55) return 4;
  if (r >= -65) return 3;
  if (r >= -75) return 2;
  if (r >= -85) return 1;
  return 0;
}

std::string ssid() { return connected() ? std::string(WiFi.SSID().c_str()) : std::string(); }

std::string ip() {
  return connected() ? std::string(WiFi.localIP().toString().c_str()) : std::string();
}

std::string apSsid() { return gApSsid; }

bool apHasPassword() { return gApPassword; }

std::vector<std::string> scan() {
  std::vector<std::pair<int, std::string>> found;
  int n = WiFi.scanNetworks(false, false);
  for (int i = 0; i < n; ++i) {
    std::string name = WiFi.SSID(i).c_str();
    if (name.empty()) continue;
    int rssi = WiFi.RSSI(i);
    auto it = std::find_if(found.begin(), found.end(),
                           [&](const auto& f) { return f.second == name; });
    if (it == found.end()) found.emplace_back(rssi, name);
    else if (rssi > it->first) it->first = rssi;
  }
  WiFi.scanDelete();
  std::sort(found.begin(), found.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
  std::vector<std::string> out;
  for (auto& f : found) out.push_back(std::move(f.second));
  return out;
}

void reconnectWithBackoff() {
  if (gNets.empty()) return;
  if (connected()) {
    if (gTrying) {
      Serial.printf("[wifi] connected, ip %s\n", WiFi.localIP().toString().c_str());
      gTrying = false;
    }
    gBackoffMs = 1000;
    gNext = 0;
    gWaitUntil = millis();  // a later drop retries at once
    return;
  }
  uint32_t now = millis();
  if (gTrying) {
    if (now - gAttemptStart < kAttemptMs) return;
    gTrying = false;
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
