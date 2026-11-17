// SPDX-License-Identifier: GPL-3.0-or-later
// Read-only Wi-Fi status: the OS owns the network (spec section 4.2). Always
// Station; connected when a non-loopback interface has IPv4 (on Linux, the
// one with the default route). Scans, the setup AP and network edits do
// nothing.
#include "net/wifi_mgr.h"

#include <Arduino.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>

#include <cstdlib>
#include <fstream>
#include <mutex>
#include <sstream>

#if defined(__linux__)
#include <linux/wireless.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace wifimgr {

namespace {

constexpr uint32_t kRefreshMs = 2000;  // the status bar asks every frame

struct Status {
  bool connected = false;
  std::string iface, ip, ssid;
  int bars = 0;
};

std::mutex gMu;
Status gStatus;
uint32_t gAt = 0;
bool gRead = false;

#if defined(__linux__)
// The interface holding the default IPv4 route, "" when there is none.
std::string defaultRouteIface() {
  std::ifstream f("/proc/net/route");
  std::string line;
  std::getline(f, line);  // header
  while (std::getline(f, line)) {
    std::istringstream in(line);
    std::string iface, dest;
    if (in >> iface >> dest && dest == "00000000") return iface;
  }
  return "";
}

std::string essidOf(const std::string& iface) {
  const int fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0) return "";
  char buf[IW_ESSID_MAX_SIZE + 1] = {};
  iwreq req{};
  std::snprintf(req.ifr_name, sizeof req.ifr_name, "%s", iface.c_str());
  req.u.essid.pointer = buf;
  req.u.essid.length = IW_ESSID_MAX_SIZE;
  const bool ok = ioctl(fd, SIOCGIWESSID, &req) == 0;
  close(fd);
  return ok ? std::string(buf, strnlen(buf, IW_ESSID_MAX_SIZE)) : "";
}

// The signal level in dBm from /proc/net/wireless; false for a wired link.
bool levelOf(const std::string& iface, int& dbm) {
  std::ifstream f("/proc/net/wireless");
  std::string line;
  while (std::getline(f, line)) {
    std::istringstream in(line);
    std::string name, st, link, level;
    if (!(in >> name >> st >> link >> level)) continue;
    if (name != iface + ":") continue;
    dbm = std::atoi(level.c_str());  // "-52." or "-52"
    if (dbm > 0) dbm -= 256;          // some drivers report it unsigned
    return true;
  }
  return false;
}
#endif

Status read() {
  Status s;
#if defined(__linux__)
  s.iface = defaultRouteIface();
  if (s.iface.empty()) return s;
#endif
  ifaddrs* list = nullptr;
  if (getifaddrs(&list) != 0) return s;
  for (ifaddrs* a = list; a; a = a->ifa_next) {
    if (!a->ifa_addr || a->ifa_addr->sa_family != AF_INET) continue;
    if ((a->ifa_flags & IFF_LOOPBACK) || !(a->ifa_flags & IFF_UP)) continue;
    if (!s.iface.empty() && s.iface != a->ifa_name) continue;
    char buf[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &reinterpret_cast<sockaddr_in*>(a->ifa_addr)->sin_addr, buf, sizeof buf);
    s.iface = a->ifa_name;
    s.ip = buf;
    s.connected = true;
    break;
  }
  freeifaddrs(list);
  if (!s.connected) return s;
  s.bars = 4;  // wired, or a link that reports no level
#if defined(__linux__)
  int dbm;
  if (levelOf(s.iface, dbm)) {
    s.bars = rc::wifiBarsFor(dbm);
    s.ssid = essidOf(s.iface);
  }
#endif
  return s;
}

Status status() {
  std::lock_guard<std::mutex> g(gMu);
  const uint32_t now = millis();
  if (!gRead || now - gAt >= kRefreshMs) {
    gStatus = read();
    gAt = now;
    gRead = true;
  }
  return gStatus;
}

}  // namespace

void begin(const rc::Settings&) {
  const Status s = status();
  Serial.printf("[wifi] system network: %s\n", s.connected ? "connected" : "not connected");
}

Mode mode() { return Mode::Station; }

bool connected() { return status().connected; }

int rssiBars() { return status().bars; }

std::string ssid() { return status().ssid; }

std::string ip() { return status().ip; }

std::string apSsid() { return ""; }

bool apHasPassword() { return false; }

void setSetupAp(bool) {}

std::vector<std::string> scan() { return {}; }

std::vector<rc::WifiScanEntry> scanDetailed() { return {}; }

void setNetworks(const std::vector<rc::WifiNet>&, const std::string&) {}

int takeJoinResult() { return 0; }

void reconnectWithBackoff() {}

}  // namespace wifimgr
