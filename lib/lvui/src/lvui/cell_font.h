#pragma once

#include <lvgl.h>

#include <optional>

#include "ui/grid.h"

namespace lvui {

// The Terminus cell font for a text size, read straight from the cell
// tables: every glyph is one cell wide, a wide emoji two (both its Noto
// halves), and its tail code point and zero-width marks none, so a string is
// as wide as rc::textWidth cells. big: the next size up (rc::bigCellSize), for
// header rows. bold: each glyph row ORed with itself 1 px right, as the grid
// renderer draws a bold cell.
const lv_font_t* font(rc::TextSize s, bool big = false, bool bold = false);

// The font for the saved text size, or for the board's fixed one if it has one.
const lv_font_t* fontFor(rc::TextSize saved, std::optional<rc::TextSize> fixed);

}  // namespace lvui

// The Comfortable font, LVGL's default (LV_FONT_DEFAULT in lv_conf.h).
extern "C" const lv_font_t lv_font_cell8x16;
