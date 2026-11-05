#pragma once

#include "ui/notify_light.h"

namespace rc {

// The Wi-Fi light (board::hasWifiLight): green while joined to a network,
// blue while the setup AP is up and nothing is joined, yellow while looking.
constexpr LedColor wifiLightColor(bool joined, bool setupAp) {
  return joined ? LedColor::Green : setupAp ? LedColor::Blue : LedColor::Yellow;
}

}  // namespace rc
