#pragma once

#include <string>
#include <vector>

#include "ui/grid.h"
#include "ui/theme.h"

namespace rc {

struct WifiNet {
  std::string ssid, password;
};

struct Settings {
  std::vector<WifiNet> wifi;
  std::string email, refreshToken, tz = "UTC0", pagePassword, apPassword;
  TextSize textSize = TextSize::Compact;
  bool drawerOpen = false, buzzer = true;
  ThemeId theme = ThemeId::Default;
  std::string lastRoom = "general";
};

constexpr size_t kMaxWifiNets = 5;

}  // namespace rc

// NVS namespace "rcfg". Wi-Fi beyond kMaxWifiNets is dropped on save.
namespace settings {
void load(rc::Settings&);
// Each saver writes only its own keys, so the UI task and the web server
// never overwrite each other's changes.
// The UI's own preferences: text size, theme, drawer, buzzer, last room.
void saveUiPrefs(const rc::Settings&);
// Web config only. Networks beyond kMaxWifiNets are dropped.
void saveWifi(const std::vector<rc::WifiNet>&);
void saveTz(const std::string& tz);
void saveSecurity(const std::string& pagePassword, const std::string& apPassword);
void saveToken(const std::string& refreshToken);
void clearToken();
// Saves the email and refresh token after a login.
void saveAccount(const std::string& email, const std::string& refreshToken);
// Erases every saved setting. The caller restarts; with no Wi-Fi saved the
// device then comes up in setup mode.
void factoryReset();
}  // namespace settings
