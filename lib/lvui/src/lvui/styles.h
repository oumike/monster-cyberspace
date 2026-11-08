#pragma once

#include <lvgl.h>

#include "ui/theme.h"

namespace lvui {

// What an object is in the UI; each maps to one style built from the theme.
enum class Role {
  Screen, Bar, Hint, Item, Focused, Selected, Input, Cursor, Drawer, DrawerEdge,
  Error, Mention, Dim, Time, Sys, Nick, SelfNick
};

// A theme's RGB565 colour as an LVGL colour.
lv_color_t color(uint16_t rgb565);

// Rewrites every style from `t` and restyles the objects that use them.
void applyTheme(const rc::Theme& t);
lv_style_t* style(Role r);
void addStyle(lv_obj_t* obj, Role r, lv_style_selector_t sel = 0);
// The last theme applied; defaultTheme() before any.
const rc::Theme& currentTheme();

}  // namespace lvui
