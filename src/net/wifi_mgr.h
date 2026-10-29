#pragma once

#include <string>
#include <vector>

#include "net/wifi_list.h"
#include "store/settings.h"

namespace wifimgr {

enum class Mode { Station, Setup };

// Setup mode raises the setup AP at 192.168.4.1: open unless apPassword is
// set. No DNS server, so nothing pops up on a joining phone. The station
// side stays up in setup mode so the page can scan for networks. It runs at
// boot with no saved networks (first boot, or after a factory reset), when
// the user turns it on (Settings), or when the saved networks have failed
// for 5 minutes (rc::ApFallback; that AP drops again once Wi-Fi is back and
// nobody is joined to it).
void begin(const rc::Settings&);
Mode mode();
bool connected();
int rssiBars();  // 0-4; 0 when not connected
std::string ssid();  // the joined network, "" when not connected
std::string ip();    // station IP, "" when not connected
std::string apSsid();
bool apHasPassword();
// Turns the setup AP on or off from the network task. Off is ignored with
// no saved networks: setup mode is then the only way in.
void setSetupAp(bool on);
// Blocking scan (a few seconds): unique SSIDs, strongest first.
std::vector<std::string> scan();
// The same scan with signal and security, for the device's Wi-Fi screen.
std::vector<rc::WifiScanEntry> scanDetailed();
// From the network task: the saved networks changed on the device. A
// non-empty joinNow is tried at once; its outcome comes from takeJoinResult.
// A network no longer saved is left; with none left the setup AP comes up.
void setNetworks(const std::vector<rc::WifiNet>& nets, const std::string& joinNow);
// 1 the joinNow network connected, -1 it failed, 0 nothing new. Read once.
int takeJoinResult();
// Call often. Runs the AP fallback, and tries the saved networks in order,
// backing off 1, 2, 4 … 60 s after each full pass that fails.
void reconnectWithBackoff();

}  // namespace wifimgr
