#pragma once

#include "ui/notify_light.h"

namespace rc {

// The Wi-Fi light (board::hasWifiLight) shows one of three states, each in
// its own colour setting: joined to a network; the setup AP up with nothing
// joined; looking for a network.
enum class WifiLight : uint8_t { Looking, SetupAp, Joined };

constexpr WifiLight wifiLightState(bool joined, bool setupAp) {
  return joined ? WifiLight::Joined : setupAp ? WifiLight::SetupAp : WifiLight::Looking;
}

}  // namespace rc
