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
// Where the finger went down for the press being handled (as the grid's tap
// map hits the down point).
lv_point_t tapPoint();

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
// A local, opaque background from a theme RGB565 value, set only when it differs.
void setBgColor(lv_obj_t* obj, uint16_t rgb565);
// Moves and sizes obj only when it differs.
void place(lv_obj_t* obj, int x, int y, int w, int h);

// A scroll bar as the grid's rc::drawScrollBar draws one: a line
// max(2, cell width / 4) px wide down the right of its last column, the track
// in barBg and the thumb in fg (rc::scrollThumb, in whole rows). Hidden when
// everything fits.
class ScrollBarView {
 public:
  void create(lv_obj_t* parent);
  // rightPx: the bar column's right edge; rows of cell.h px from the top.
  // bigH > 0: the rows alternate bigH and cell.h px, a big one first
  // (rc::EntryList's big heads).
  void sync(int rightPx, Cell cell, int rows, int total, int visible, int first, int bigH = 0);
  lv_obj_t* track() const { return track_; }
  lv_obj_t* thumb() const { return thumb_; }

 private:
  lv_obj_t *track_ = nullptr, *thumb_ = nullptr;
};

// s cut to max code points with a trailing "…", as the status bar and
// drawer cut theirs.
std::u32string fitCells(std::u32string s, int max);

// The KeyEvent a row tap makes: Activate on `list` at `index`, then Select
// (a long press: then Palette).
rc::KeyEvent rowKey(rc::ListId list, int index, bool hold);

}  // namespace lvui
