#include "net/wifi_list.h"

#include <algorithm>
#include <cstdlib>

namespace rc {

int wifiBarsFor(int rssi) {
  if (rssi >= -55) return 4;
  if (rssi >= -65) return 3;
  if (rssi >= -75) return 2;
  if (rssi >= -85) return 1;
  return 0;
}

bool validWifiSsid(std::string_view ssid) { return !ssid.empty() && ssid.size() <= 32; }

bool validWifiPassword(std::string_view pw) { return pw.empty() || (pw.size() >= 8 && pw.size() <= 63); }

WifiSave saveWifiNet(std::vector<WifiNet>& nets, const std::string& ssid, const std::string& password) {
  for (WifiNet& n : nets)
    if (n.ssid == ssid) {
      n.password = password;
      return WifiSave::Updated;
    }
  if (nets.size() >= kMaxWifiNets) return WifiSave::Full;
  nets.push_back(WifiNet{ssid, password});
  return WifiSave::Added;
}

bool forgetWifiNet(std::vector<WifiNet>& nets, const std::string& ssid) {
  const size_t before = nets.size();
  nets.erase(std::remove_if(nets.begin(), nets.end(), [&](const WifiNet& n) { return n.ssid == ssid; }),
             nets.end());
  return nets.size() != before;
}

// Each entry is "<rssi>,<0|1>,<bytes>:<ssid>", so the SSID needs no escaping.
std::string encodeWifiScan(const std::vector<WifiScanEntry>& scan) {
  std::string out;
  for (const WifiScanEntry& e : scan) {
    out += std::to_string(e.rssi);
    out += e.secure ? ",1," : ",0,";
    out += std::to_string(e.ssid.size());
    out += ':';
    out += e.ssid;
  }
  return out;
}

namespace {
bool readInt(std::string_view s, size_t& pos, char end, long& out) {
  const size_t stop = s.find(end, pos);
  if (stop == std::string_view::npos || stop == pos || stop - pos > 6) return false;
  const std::string num(s.substr(pos, stop - pos));
  char* tail = nullptr;
  out = std::strtol(num.c_str(), &tail, 10);
  if (*tail) return false;
  pos = stop + 1;
  return true;
}
}  // namespace

bool decodeWifiScan(std::string_view text, std::vector<WifiScanEntry>& out) {
  out.clear();
  size_t pos = 0;
  while (pos < text.size()) {
    long rssi = 0, secure = 0, len = 0;
    if (!readInt(text, pos, ',', rssi) || !readInt(text, pos, ',', secure) || !readInt(text, pos, ':', len) ||
        len < 0 || pos + size_t(len) > text.size()) {
      out.clear();
      return false;
    }
    out.push_back(WifiScanEntry{std::string(text.substr(pos, size_t(len))), int(rssi), secure != 0});
    pos += size_t(len);
  }
  return true;
}

std::vector<WifiRow> wifiRows(const std::vector<WifiScanEntry>& scan, const std::vector<WifiNet>& saved,
                              const std::string& connected) {
  auto isSaved = [&](const std::string& ssid) {
    return std::any_of(saved.begin(), saved.end(), [&](const WifiNet& n) { return n.ssid == ssid; });
  };
  std::vector<WifiScanEntry> seen;
  for (const WifiScanEntry& e : scan) {
    if (e.ssid.empty()) continue;
    auto it = std::find_if(seen.begin(), seen.end(), [&](const WifiScanEntry& s) { return s.ssid == e.ssid; });
    if (it == seen.end()) seen.push_back(e);
    else if (e.rssi > it->rssi) *it = e;
  }
  std::stable_sort(seen.begin(), seen.end(), [&](const WifiScanEntry& a, const WifiScanEntry& b) {
    const bool ca = !connected.empty() && a.ssid == connected, cb = !connected.empty() && b.ssid == connected;
    if (ca != cb) return ca;
    return a.rssi > b.rssi;
  });
  std::vector<WifiRow> rows;
  for (const WifiScanEntry& e : seen)
    rows.push_back(WifiRow{e.ssid, wifiBarsFor(e.rssi), e.secure, isSaved(e.ssid),
                           !connected.empty() && e.ssid == connected});
  for (const WifiNet& n : saved) {
    if (std::any_of(rows.begin(), rows.end(), [&](const WifiRow& r) { return r.ssid == n.ssid; })) continue;
    WifiRow r{n.ssid, -1, !n.password.empty(), true, n.ssid == connected};
    if (r.connected) rows.insert(rows.begin(), r);
    else rows.push_back(r);
  }
  return rows;
}

}  // namespace rc
