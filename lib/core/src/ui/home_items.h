#pragma once

#include <vector>

#include "feature_flags.h"

namespace rc {

// Home's entries. Messages (the last room) and Rooms are always there;
// Profile is your own profile (with People built).
enum class HomeItem { Unread, Messages, Rooms, Cmail, Feed, Guilds, Profile, Settings };

// Home's tiles for the built features, in display order.
std::vector<HomeItem> homeItems(const Features& f);

// Whether Unread, first, takes a tile row of its own (ui/home_tiles.h
// spanFirst): only when the tiles after it then fill their rows of two, so no
// tile is left alone on the last row.
bool homeSpanFirst(const std::vector<HomeItem>& items);

}  // namespace rc
