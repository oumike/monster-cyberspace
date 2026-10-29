#pragma once

#include "ui/grid.h"
#include "ui/theme.h"

namespace rc {

// The Up / Select / Down column boards with board::sideNav() put at the right
// of lists (messages, the feed, C-Mail), where picking a row by touch is a
// chore (e-paper). The middle button acts as Enter on the selected item.

// Columns for a strip about 18 px wide in cells cellW px wide; at least 2.
int sideNavCols(int cellW);

// Three outlined buttons stacked over rows [top, bottom) in the right `cols`
// columns, each registered as a Fixed tap sending Up, Select or Down.
void renderSideNav(Grid& g, int top, int bottom, int cols, const Theme& t);

}  // namespace rc
