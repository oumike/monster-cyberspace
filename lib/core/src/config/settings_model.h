#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "config/phrases.h"
#include "ui/grid.h"
#include "ui/kb_light.h"
#include "ui/notify_light.h"
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
  bool spellCheck = true;      // spell suggestions while typing (ui/widgets/spell_strip.h)
  // Boards with a lit keyboard (board::hasKbLight); the saved value falls back
  // to board::kbLightDefault().
  KbLight kbLight = KbLight::Off;
  // Boards with a new-message light (board::hasNotifyLight): its colour, and
  // how long it blinks after the last message (ui/notify_light.h); 0 until read.
  LedColor notifyLight = LedColor::Blue;
  uint16_t notifyTimeoutSecs = 15;
  ThemeId theme = ThemeId::Default;
  uint8_t rotation = 0;  // touch boards: see ui/rotation.h
  std::string lastRoom = "general";
  // tz is the POSIX string applied; tzName its IANA name from the zone list,
  // empty for a custom string.
  std::string tzName = "Etc/UTC";
  std::string ntpServer = "pool.ntp.org";  // SNTP server 0; time.google.com is server 1
  // Canned replies for boards that can't type (config/phrases.h).
  std::vector<std::string> phrases = defaultPhrases();
  // Serve web config on the joined network; the setup AP always serves it.
  bool webConfig = true;
  // Boards that sleep on battery (power/power_policy.h): idle seconds before
  // sleeping, and minutes between refresh wakes (0: only a button wakes it).
  uint32_t idleSecs = 300;
  uint32_t wakeMins = 15;
};

constexpr size_t kMaxWifiNets = 5;

}  // namespace rc
