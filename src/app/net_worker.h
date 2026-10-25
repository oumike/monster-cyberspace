#pragma once

#include "store/settings.h"

namespace rc::networker {

// Starts Wi-Fi (wifimgr::begin) and the network task on core 0 (12 KB
// stack). It serves commands from the queue, keeps the open room's streams
// and presence alive, and polls rooms and mentions every 60 s.
void start(const Settings&);

}  // namespace rc::networker
