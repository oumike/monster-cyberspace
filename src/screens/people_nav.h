#pragma once

#include <string>

namespace rc::people {

// Opens a profile, remembering where it was opened from, so Back on the
// Profile screen returns there (spec: People). Up to 8 places are kept.
void openProfile(const std::string& username);
// Back from a Profile: the place it was opened from, else Home.
void back();

}  // namespace rc::people
