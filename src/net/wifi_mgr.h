#pragma once

#include <string>
#include <vector>

#include "store/settings.h"

namespace wifimgr {

enum class Mode { Station, Setup };

// Setup mode runs with no saved networks (first boot, or after a factory
// reset). It raises the setup AP at 192.168.4.1: open unless apPassword is
// set. No DNS server, so nothing pops up on a joining phone. The station
// side stays up in setup mode so the page can scan for networks.
void begin(const rc::Settings&);
Mode mode();
bool connected();
int rssiBars();  // 0-4; 0 when not connected
std::string ssid();  // the joined network, "" when not connected
std::string ip();    // station IP, "" when not connected
std::string apSsid();
bool apHasPassword();
// Blocking scan (a few seconds): unique SSIDs, strongest first.
std::vector<std::string> scan();
// Call often. Tries the saved networks in order, backing off 1, 2, 4 … 60 s
// after each full pass that fails.
void reconnectWithBackoff();

}  // namespace wifimgr
