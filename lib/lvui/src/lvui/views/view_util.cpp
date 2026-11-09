#include "lvui/views/view_util.h"

#include <cstring>

#include <algorithm>

#include "lvui/cell_font.h"
#include "src/core/lv_obj_class_private.h"
#include "lvui/styles.h"
#include "ui/widgets/scroll_bar.h"
#include "util/utf8.h"

namespace lvui {

namespace {

Header shownHeader;

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

// A label that asks for no extra draw area. lv_label adds font_h / 4 for
// italic overhang, which allocates spec_attr on every label; a CLIP label
// draws inside its content area anyway, so the pixels are the same.
void lineLabelEvent(const lv_obj_class_t* cls, lv_event_t* e);

const lv_obj_class_t kLineLabelClass = [] {
  lv_obj_class_t c{};
  c.base_class = &lv_label_class;
  c.event_cb = lineLabelEvent;
  c.width_def = LV_SIZE_CONTENT;
  c.height_def = LV_SIZE_CONTENT;
  c.name = "line_label";
  return c;
}();

void lineLabelEvent(const lv_obj_class_t*, lv_event_t* e) {
  // Skip lv_label's handler for this one event; lv_obj's still adds outlines.
  if (lv_event_get_code(e) == LV_EVENT_REFR_EXT_DRAW_SIZE) {
    lv_obj_event_base(&lv_label_class, e);
    return;
  }
  lv_obj_event_base(&kLineLabelClass, e);
}

}  // namespace

void addTap(lv_obj_t* obj, Tappable* v) {
  lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(obj, tapCb, LV_EVENT_PRESSED, v);
  lv_obj_add_event_cb(obj, tapCb, LV_EVENT_LONG_PRESSED, v);
  lv_obj_add_event_cb(obj, tapCb, LV_EVENT_CLICKED, v);
}

lv_point_t tapPoint() { return downAt; }

Header touchHeader(rc::TextSize s) {
  Header h;
  h.px = rc::touchHeader(0, s).px;
  h.extra = std::max(0, h.px - rc::cellSize(s).h);
  h.font = font(s, true);
  return h;
}

void setHeader(const Header& h) { shownHeader = h; }
const Header& header() { return shownHeader; }

int headerStrip(lv_obj_t* parent) {
  const int extra = shownHeader.extra;
  if (extra <= 0 || !parent) return 0;
  lv_obj_update_layout(parent);
  lv_area_t a;
  lv_obj_get_coords(parent, &a);
  const int top = a.y1 + lv_obj_get_style_pad_top(parent, LV_PART_MAIN) +
                  lv_obj_get_style_border_width(parent, LV_PART_MAIN);
  if (top != extra) return 0;
  // The padded root draws its own padding; a box inside it must not clip.
  if (lv_obj_get_style_pad_top(parent, LV_PART_MAIN) == 0) lv_obj_add_flag(parent, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
  return extra;
}

lv_obj_t* plainBox(lv_obj_t* parent) {
  lv_obj_t* o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
  return o;
}

lv_obj_t* lineLabel(lv_obj_t* parent) {
  lv_obj_t* l = lv_obj_class_create_obj(&kLineLabelClass, parent);
  lv_obj_class_init_obj(l);
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
  if (!obj) return;
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

void ScrollBarView::sync(int rightPx, Cell cell, int rows, int total, int visible, int first, int bigH) {
  const bool on = rows > 0 && rc::needsScrollBar(total, visible);
  setHidden(track_, !on);
  setHidden(thumb_, !on);
  if (!on) return;
  const rc::Theme& t = currentTheme();
  const int w = std::max(2, cell.w / 4);
  const rc::ScrollThumb th = rc::scrollThumb(rows, total, visible, first);
  auto rowY = [&](int r) { return bigH > 0 ? r / 2 * (bigH + cell.h) + r % 2 * bigH : r * cell.h; };
  place(track_, rightPx - w, 0, w, rowY(rows));
  place(thumb_, rightPx - w, rowY(th.pos), w, rowY(th.pos + th.len) - rowY(th.pos));
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
