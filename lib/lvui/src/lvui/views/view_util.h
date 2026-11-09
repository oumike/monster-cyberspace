#pragma once

#include <lvgl.h>

#include <string>
#include <string_view>

#include "input/key_event.h"
#include "lvui/input.h"
#include "ui/grid.h"

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

// Draws a box's contents itself (paintedBox): rows of text and fills in one
// object rather than an object each, for boards without PSRAM.
class Painter {
 public:
  // In the box's LV_EVENT_DRAW_MAIN; layer->_clip_area is the box's clip.
  virtual void paint(lv_obj_t* box, lv_layer_t* layer) = 0;

 protected:
  ~Painter() = default;
};
// A plainBox that calls p->paint when it draws. It takes no event list, so
// it costs what a plainBox does.
lv_obj_t* paintedBox(lv_obj_t* parent, Painter* p);
// Draws text as a lineLabel of width w at (x, y) would (one line, never
// wrapped), one cell high, in fg and font, clipped to that box and to clip
// (the parent's), and returns w. w < 0: the label's own width
// (LV_SIZE_CONTENT), which it returns.
int32_t paintLabel(lv_layer_t* layer, lv_obj_t* box, const char* text, int32_t x, int32_t y, int32_t w,
                   int32_t h, lv_color_t fg, const lv_font_t* font, const lv_area_t& clip);
// The width a LV_SIZE_CONTENT lineLabel of text in font takes.
int32_t textWidth(const char* text, const lv_font_t* font);
// Fills, or outlines, an area as an object of that size with the style
// would: bg_opa 0 skips the fill, border 0 the outline.
void paintRect(lv_layer_t* layer, const lv_area_t& a, lv_color_t bg, lv_opa_t bgOpa, lv_color_t border,
               int32_t borderW);

// The cell an object's font draws: every glyph is one cell wide.
struct Cell {
  int w, h;
};
Cell cellOf(lv_obj_t* obj);

// Sets the text only when it differs, so an unchanged sync redraws nothing.
void setText(lv_obj_t* label, const std::string& s);
void setText(lv_obj_t* label, std::u32string_view s);
// Null is fine: a part not made yet (made with its first show) is hidden.
void setHidden(lv_obj_t* obj, bool hidden);
void setChecked(lv_obj_t* obj, bool on);
// A local text colour from a theme RGB565 value, set only when it differs.
void setTextColor(lv_obj_t* obj, uint16_t rgb565);
// A local, opaque background from a theme RGB565 value, set only when it differs.
void setBgColor(lv_obj_t* obj, uint16_t rgb565);
// Moves and sizes obj only when it differs.
void place(lv_obj_t* obj, int x, int y, int w, int h);

// The touch status bar (the grid's rc::touchHeader): row 0 px tall in the next
// text size up, so every row below it sits extra px lower. The screen root
// pads its top by extra; a TopBar reaches up over that strip. px 0: off.
struct Header {
  int px = 0, extra = 0;
  const lv_font_t* font = nullptr;
};
// The header for text size s: rc::touchHeader's height, font(s, true).
Header touchHeader(rc::TextSize s);
// Set before a screen is built (ScreenHost pads the new root by it).
void setHeader(const Header& h);
const Header& header();
// header().extra when parent's content starts that far down the display (the
// screen root's, or a full-size box on it), else 0: how far a status bar or
// a full-screen backdrop in parent reaches above it. Such a parent stops
// clipping, so what reaches up still draws.
int headerStrip(lv_obj_t* parent);

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
