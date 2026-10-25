#pragma once

#include <cstdint>

#include "app/messages.h"

namespace rc {

constexpr size_t kQueueDepth = 32;

// Thread-safe queues between the UI task and the network side, depth 32 each.
// A full command queue refuses the command. A full event queue drops its
// oldest RoomChanged (the next one redraws anyway), else its oldest event.
bool postCommand(Command c);
bool nextCommand(Command& c, uint32_t waitMs);
bool postEvent(Event e);
bool nextEvent(Event& e);

}  // namespace rc
