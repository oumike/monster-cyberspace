#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "store/settings.h"
#include "ui/grid.h"
#include "ui/theme.h"

namespace rc {

// An unread cIRC mention, for the Unread screen.
struct MentionNote {
  std::string slug, text;
  int64_t at = 0;  // epoch ms
};

// UI-side state, touched only by the UI task.
struct AppState {
  Settings cfg;
  int conn = 0;          // 0 offline, 1 reconnecting, 2 live
  int auth = -1;         // -1 unknown, 0 signed out, 1 signed in, 2 email unverified
  std::string user;      // signed-in username
  std::string authNote;  // the last auth error, for the login form
  // Unread mentions, newest last; a room's go when it opens. Their count is
  // the unread mentions.
  std::vector<MentionNote> mentions;
};

AppState& app();

// The theme the user picked (Settings or web config).
const Theme& currentTheme();

// Row 0 with the connection marker, Wi-Fi bars, battery and clock. `menu` means
// the screen has a Ctrl menu, so a touch board draws a `⋯` at the right.
void drawStatus(Grid& g, const Theme& t, std::u32string_view title, int count, bool menu = false);

// Wi-Fi and address details that differ in the demo build (RC_FAKE_API).
bool wifiUp();
std::string wifiSsid();
std::string wifiIp();
// The setup AP (Settings, or the 5-minute fallback).
bool setupApUp();
std::string setupApSsid();
bool setupApHasPassword();
int wifiBars();

// A local-time "HH:MM", or "--:--" before SNTP has set the clock.
std::string clockText();
int64_t nowEpochMs();
// The battery percent to show (-1 unknown), steadied by a BatteryMeter. UI
// task only; it reads the board every 2 s.
int batteryShown();

}  // namespace rc
