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
// Saves the email and refresh token after a login.
void saveAccount(const std::string& email, const std::string& refreshToken);
// Erases every saved setting. The caller restarts; with no Wi-Fi saved the
// device then comes up in setup mode.
void factoryReset();
}  // namespace settings
