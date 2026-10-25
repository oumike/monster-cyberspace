#pragma once

#include <string_view>

#include "ui/grid.h"
#include "ui/theme.h"

namespace rc {

// Row 0: " title count" on the left; "marker bars battery% clock " on the
// right. count < 0 or batteryPct < 0 hides that field. Connection markers:
// ● live, ◌ reconnecting, ✕ offline. wifiBars is 0–4.
void renderStatusBar(Grid& g, const Theme& t, std::u32string_view title, int count,
                     char32_t connMarker, int wifiBars, int batteryPct, std::string_view clock);

}  // namespace rc
