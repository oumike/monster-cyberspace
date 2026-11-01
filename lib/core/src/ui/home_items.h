#pragma once

#include <vector>

#include "features.h"

namespace rc {

// Home's entries. Messages (the last room) and Rooms are always there.
enum class HomeItem { Unread, Messages, Rooms, Cmail, Feed, Guilds, Settings };

// Home's tiles for the built features, in display order.
std::vector<HomeItem> homeItems(const Features& f);

}  // namespace rc
