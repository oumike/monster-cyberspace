#include "lvui/views/view_util.h"

#include <cstring>

#include <algorithm>

#include "lvui/styles.h"
#include "ui/widgets/scroll_bar.h"
#include "util/utf8.h"

namespace lvui {

namespace {

// One pointer, so one press at a time: set when its long press acted.
bool longSent = false;
lv_point_t downAt{0, 0};

void tapCb(lv_event_t* e) {
  auto* v = static_cast<Tappable*>(lv_event_get_user_data(e));
  lv_obj_t* obj = static_cast<lv_obj_t*>(lv_event_get_current_target(e));
  switch (lv_event_get_code(e)) {
    case LV_EVENT_PRESSED:
      longSent = false;
      if (lv_indev_t* in = lv_indev_active()) lv_indev_get_point(in, &downAt);
      break;
    case LV_EVENT_LONG_PRESSED:
      longSent = true;
      v->tapped(obj, true);
      break;
    case LV_EVENT_CLICKED:
      if (!longSent) v->tapped(obj, false);
      longSent = false;
      break;
    default:
      break;
  }
}

}  // namespace

void addTap(lv_obj_t* obj, Tappable* v) {
  lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(obj, tapCb, LV_EVENT_PRESSED, v);
  lv_obj_add_event_cb(obj, tapCb, LV_EVENT_LONG_PRESSED, v);
  lv_obj_add_event_cb(obj, tapCb, LV_EVENT_CLICKED, v);
}

lv_point_t tapPoint() { return downAt; }

lv_obj_t* plainBox(lv_obj_t* parent) {
  lv_obj_t* o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
  return o;
}

lv_obj_t* lineLabel(lv_obj_t* parent) {
  lv_obj_t* l = lv_label_create(parent);
  lv_obj_remove_style_all(l);
  lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_CLIP);
  lv_label_set_text_static(l, "");
  lv_obj_set_height(l, cellOf(l).h);
  return l;
}

Cell cellOf(lv_obj_t* obj) {
  const lv_font_t* f = lv_obj_get_style_text_font(obj, LV_PART_MAIN);
  return {static_cast<int>(lv_font_get_glyph_width(f, ' ', 0)), lv_font_get_line_height(f)};
}

void setText(lv_obj_t* label, const std::string& s) {
  if (strcmp(lv_label_get_text(label), s.c_str()) != 0) lv_label_set_text(label, s.c_str());
}

void setText(lv_obj_t* label, std::u32string_view s) { setText(label, rc::u32ToUtf8(s)); }

void setHidden(lv_obj_t* obj, bool hidden) {
  if (lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN) != hidden) lv_obj_set_flag(obj, LV_OBJ_FLAG_HIDDEN, hidden);
}

void setChecked(lv_obj_t* obj, bool on) {
  if (lv_obj_has_state(obj, LV_STATE_CHECKED) != on) lv_obj_set_state(obj, LV_STATE_CHECKED, on);
}

void setTextColor(lv_obj_t* obj, uint16_t rgb565) {
  const lv_color_t c = color(rgb565);
  lv_style_value_t v;
  if (lv_obj_get_local_style_prop(obj, LV_STYLE_TEXT_COLOR, &v, LV_PART_MAIN) == LV_STYLE_RES_FOUND &&
      lv_color_eq(v.color, c))
    return;
  lv_obj_set_style_text_color(obj, c, LV_PART_MAIN);
}

void setBgColor(lv_obj_t* obj, uint16_t rgb565) {
  const lv_color_t c = color(rgb565);
  lv_style_value_t v;
  if (lv_obj_get_local_style_prop(obj, LV_STYLE_BG_COLOR, &v, LV_PART_MAIN) != LV_STYLE_RES_FOUND ||
      !lv_color_eq(v.color, c))
    lv_obj_set_style_bg_color(obj, c, LV_PART_MAIN);
  if (lv_obj_get_local_style_prop(obj, LV_STYLE_BG_OPA, &v, LV_PART_MAIN) != LV_STYLE_RES_FOUND ||
      v.num != LV_OPA_COVER)
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
}

void place(lv_obj_t* obj, int x, int y, int w, int h) {
  if (lv_obj_get_x_aligned(obj) != x || lv_obj_get_y_aligned(obj) != y) lv_obj_set_pos(obj, x, y);
  if (lv_obj_get_style_width(obj, LV_PART_MAIN) != w) lv_obj_set_width(obj, w);
  if (lv_obj_get_style_height(obj, LV_PART_MAIN) != h) lv_obj_set_height(obj, h);
}

void ScrollBarView::create(lv_obj_t* parent) {
  track_ = plainBox(parent);
  thumb_ = plainBox(parent);
  lv_obj_add_flag(track_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(thumb_, LV_OBJ_FLAG_HIDDEN);
}

void ScrollBarView::sync(int rightPx, Cell cell, int rows, int total, int visible, int first) {
  const bool on = rows > 0 && rc::needsScrollBar(total, visible);
  setHidden(track_, !on);
  setHidden(thumb_, !on);
  if (!on) return;
  const rc::Theme& t = currentTheme();
  const int w = std::max(2, cell.w / 4);
  const rc::ScrollThumb th = rc::scrollThumb(rows, total, visible, first);
  place(track_, rightPx - w, 0, w, rows * cell.h);
  place(thumb_, rightPx - w, th.pos * cell.h, w, th.len * cell.h);
  setBgColor(track_, t.barBg);
  setBgColor(thumb_, t.fg);
}

std::u32string fitCells(std::u32string s, int max) {
  if (static_cast<int>(s.size()) > max && max > 0) s = s.substr(0, max - 1) + U"\u2026";
  return s;
}

rc::KeyEvent rowKey(rc::ListId list, int index, bool hold) {
  rc::KeyEvent e;
  e.action = rc::Action::Activate;
  e.index = static_cast<int16_t>(index);
  e.target = static_cast<uint8_t>(list);
  e.then = hold ? rc::Action::Palette : rc::Action::Select;
  return e;
}

}  // namespace lvui
