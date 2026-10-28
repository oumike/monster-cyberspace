#pragma once

#include <cstdint>
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
  bool autoUpdate = false;  // install a newer release without asking
  uint32_t screenOnSecs = 30;  // screen timeout (ui/screen_timeout.h); 0 never
  bool screenSaver = true;     // at the timeout: the screensaver, else screen off
  ThemeId theme = ThemeId::Default;
  uint8_t rotation = 0;  // touch boards: see ui/rotation.h
  std::string lastRoom = "general";
  // tz is the POSIX string applied; tzName its IANA name from the zone list,
  // empty for a custom string.
  std::string tzName = "Etc/UTC";
  std::string ntpServer = "pool.ntp.org";  // SNTP server 0; time.google.com is server 1
};

constexpr size_t kMaxWifiNets = 5;

}  // namespace rc
