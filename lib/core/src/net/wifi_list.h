#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "config/settings_model.h"

namespace rc {

// One network a scan found.
struct WifiScanEntry {
  std::string ssid;
  int rssi = -100;
  bool secure = true;
};

// 0-4 signal bars for an RSSI in dBm.
int wifiBarsFor(int rssi);
bool validWifiSsid(std::string_view ssid);          // 1-32 bytes
bool validWifiPassword(std::string_view password);  // empty (open) or 8-63

enum class WifiSave { Added, Updated, Full };
// Adds the network, or updates the password of a saved one. Full: already
// kMaxWifiNets saved and this is a new one; nets is unchanged.
WifiSave saveWifiNet(std::vector<WifiNet>& nets, const std::string& ssid, const std::string& password);
// False when it wasn't saved.
bool forgetWifiNet(std::vector<WifiNet>& nets, const std::string& ssid);

// A scan as one string, for an Event between tasks. Any SSID bytes survive.
std::string encodeWifiScan(const std::vector<WifiScanEntry>& scan);
// False (and out cleared) on malformed text.
bool decodeWifiScan(std::string_view text, std::vector<WifiScanEntry>& out);

// A row of the on-device Wi-Fi list.
struct WifiRow {
  std::string ssid;
  int bars = -1;  // -1: saved but not in range
  bool secure = true;
  bool saved = false;
  bool connected = false;
};
// The connected network first, then the rest in range, strongest first (each
// name once), then saved networks out of range in their saved order.
std::vector<WifiRow> wifiRows(const std::vector<WifiScanEntry>& scan, const std::vector<WifiNet>& saved,
                              const std::string& connected);

}  // namespace rc
