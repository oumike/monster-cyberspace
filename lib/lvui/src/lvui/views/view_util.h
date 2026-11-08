#pragma once

#include <lvgl.h>

#include <string>
#include <string_view>

#include "input/key_event.h"
#include "lvui/input.h"

namespace lvui {

// A view whose objects take taps. tapped() gets the object the tap was added
// to; hold is a long press. A press is a tap or a long press, never both.
class Tappable {
 public:
  virtual void tapped(lv_obj_t* obj, bool hold) = 0;

 protected:
  ~Tappable() = default;
};

// Makes obj clickable and routes its taps to v.
void addTap(lv_obj_t* obj, Tappable* v);

// A bare object or label: no theme styles, no scrolling, no padding.
lv_obj_t* plainBox(lv_obj_t* parent);
// A one-line label, clipped (as the grid cuts text at a width), one cell high.
lv_obj_t* lineLabel(lv_obj_t* parent);

// The cell an object's font draws: every glyph is one cell wide.
struct Cell {
  int w, h;
};
Cell cellOf(lv_obj_t* obj);

// Sets the text only when it differs, so an unchanged sync redraws nothing.
void setText(lv_obj_t* label, const std::string& s);
void setText(lv_obj_t* label, std::u32string_view s);
void setHidden(lv_obj_t* obj, bool hidden);
void setChecked(lv_obj_t* obj, bool on);
// A local text colour from a theme RGB565 value, set only when it differs.
void setTextColor(lv_obj_t* obj, uint16_t rgb565);

// s cut to max code points with a trailing "…", as the status bar and
// drawer cut theirs.
std::u32string fitCells(std::u32string s, int max);

// The KeyEvent a row tap makes: Activate on `list` at `index`, then Select
// (a long press: then Palette).
rc::KeyEvent rowKey(rc::ListId list, int index, bool hold);

}  // namespace lvui
