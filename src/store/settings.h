#pragma once

#include "config/settings_model.h"

// NVS namespace "rcfg". Wi-Fi beyond kMaxWifiNets is dropped on save.
namespace settings {
void load(rc::Settings&);
// Each saver writes only its own keys, so the UI task and the web server
// never overwrite each other's changes.
// The UI's own preferences: text size, theme, orientation, drawer, buzzer,
// keyboard light, last room.
void saveUiPrefs(const rc::Settings&);
// Web config only. Networks beyond kMaxWifiNets are dropped.
void saveWifi(const std::vector<rc::WifiNet>&);
// The device's Settings and web config. tzName is empty for a custom string.
void saveTz(const std::string& tz, const std::string& tzName);
void saveNtpServer(const std::string& host);
void saveSecurity(const std::string& pagePassword, const std::string& apPassword);
// The device's Settings and web config: install updates without asking.
void saveAutoUpdate(bool on);
// The device's Settings and web config: the screen timeout and what it does.
void saveScreen(uint32_t onSecs, bool saver);
// The device's Settings and web config: the keyboard light.
void saveKbLight(rc::KbLight level);
// The device's Settings and web config: the new-message light's colour and
// how long it blinks (board::hasNotifyLight).
void saveNotifyLight(rc::LedColor color);
void saveNotifyTimeout(uint16_t secs);
// The device's Settings and web config: spell suggestions while typing.
void saveSpellCheck(bool on);
// Friends added from a message's menu, kept on the device.
void saveFriends(const std::vector<std::string>& friends);
// Web config: the canned replies (config/phrases.h).
void savePhrases(const std::vector<std::string>& phrases);
// The device's Settings and web config: serve web config on the joined network.
void saveWebConfig(bool on);
// Web config (boards that sleep on battery): idle before sleep, wake period.
void savePower(uint32_t idleSecs, uint32_t wakeMins);
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
