#pragma once

#include <string>
#include <vector>

#include "ui/grid.h"

namespace rc {

struct WifiNet {
  std::string ssid, password;
};

struct Settings {
  std::vector<WifiNet> wifi;
  std::string email, refreshToken, tz = "UTC0", pagePassword, apPassword;
  TextSize textSize = TextSize::Compact;
  bool drawerOpen = false, buzzer = true;
  std::string lastRoom = "general";
};

constexpr size_t kMaxWifiNets = 5;

}  // namespace rc

// NVS namespace "rcfg". Wi-Fi beyond kMaxWifiNets is dropped on save.
namespace settings {
void load(rc::Settings&);
void save(const rc::Settings&);
void saveToken(const std::string& refreshToken);
void clearToken();
// Erases every saved setting. The caller restarts; with no Wi-Fi saved the
// device then comes up in setup mode.
void factoryReset();
}  // namespace settings
