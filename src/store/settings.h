#pragma once

#include "config/settings_model.h"

// NVS namespace "rcfg". Wi-Fi beyond kMaxWifiNets is dropped on save.
namespace settings {
void load(rc::Settings&);
// Each saver writes only its own keys, so the UI task and the web server
// never overwrite each other's changes.
// The UI's own preferences: text size, theme, orientation, drawer, buzzer,
// last room.
void saveUiPrefs(const rc::Settings&);
// Web config only. Networks beyond kMaxWifiNets are dropped.
void saveWifi(const std::vector<rc::WifiNet>&);
// The device's Settings and web config. tzName is empty for a custom string.
void saveTz(const std::string& tz, const std::string& tzName);
void saveNtpServer(const std::string& host);
void saveSecurity(const std::string& pagePassword, const std::string& apPassword);
void saveToken(const std::string& refreshToken);
void clearToken();
// Saves the email and refresh token after a login.
void saveAccount(const std::string& email, const std::string& refreshToken);
// Restore path: writes every field from s. An empty refreshToken clears the
// saved token instead of writing one.
void saveAll(const rc::Settings& s);
// Erases every saved setting. The caller restarts; with no Wi-Fi saved the
// device then comes up in setup mode.
void factoryReset();
}  // namespace settings
