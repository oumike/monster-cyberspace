#include "lvui/views/view_util.h"

#include <cstring>

#include "lvui/styles.h"
#include "util/utf8.h"

namespace lvui {

namespace {

// One pointer, so one press at a time: set when its long press acted.
bool longSent = false;

void tapCb(lv_event_t* e) {
  auto* v = static_cast<Tappable*>(lv_event_get_user_data(e));
  lv_obj_t* obj = static_cast<lv_obj_t*>(lv_event_get_current_target(e));
  switch (lv_event_get_code(e)) {
    case LV_EVENT_PRESSED:
      longSent = false;
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
