#include "lvui/styles.h"

namespace lvui {

namespace {

constexpr int kRoles = static_cast<int>(Role::SelfNick) + 1;

lv_style_t styles[kRoles];
bool ready = false;
// Set on first use: the theme tables live in another file, so not at static init.
bool applied = false;
rc::Theme current{};
unsigned serial = 0;

// Every role starts from the terminal look: square, no padding, no border.
// Without the default theme only these styles and local ones exist, so the
// zeros LVGL already defaults to are left out; the border stays, as a later
// role's 0 overrides an earlier role's rule.
void base(lv_style_t* s) {
  lv_style_reset(s);
#if RC_LV_THEME
  lv_style_set_radius(s, 0);
  lv_style_set_pad_all(s, 0);
  lv_style_set_pad_row(s, 0);
  lv_style_set_pad_column(s, 0);
  lv_style_set_outline_width(s, 0);
  lv_style_set_shadow_width(s, 0);
#endif
  lv_style_set_border_width(s, 0);
}

void fill(lv_style_t* s, uint16_t bg, uint16_t fg) {
  lv_style_set_bg_color(s, color(bg));
  lv_style_set_bg_opa(s, LV_OPA_COVER);
  lv_style_set_text_color(s, color(fg));
}

void text(lv_style_t* s, uint16_t fg) { lv_style_set_text_color(s, color(fg)); }

// Today's cursor: a 1 px box in selectBg over the row, which keeps its colours.
void outline(lv_style_t* s, const rc::Theme& t) {
  lv_style_set_bg_opa(s, LV_OPA_TRANSP);
  lv_style_set_border_color(s, color(t.selectBg));
  lv_style_set_border_width(s, 1);
  lv_style_set_border_opa(s, LV_OPA_COVER);
  lv_style_set_border_side(s, LV_BORDER_SIDE_FULL);
}

lv_style_t* at(Role r) { return &styles[static_cast<int>(r)]; }

void ensure() {
  if (applied) return;
  current = rc::defaultTheme();
  applied = true;
}

void build(const rc::Theme& t) {
  if (!ready) {
    for (lv_style_t& s : styles) lv_style_init(&s);
    ready = true;
  }
  for (lv_style_t& s : styles) base(&s);
  fill(at(Role::Screen), t.bg, t.fg);
  fill(at(Role::Bar), t.barBg, t.barFg);
  // An outlined theme's bar is the page colour: a rule sets it apart.
  if (t.outlined) {
    lv_style_set_border_color(at(Role::Bar), color(t.barFg));
    lv_style_set_border_width(at(Role::Bar), 1);
    lv_style_set_border_side(at(Role::Bar), LV_BORDER_SIDE_BOTTOM);
  }
  fill(at(Role::Hint), t.hintBg, t.hintFg);
  text(at(Role::Item), t.fg);
  outline(at(Role::Focused), t);
  if (t.outlined) {
    outline(at(Role::Selected), t);
    text(at(Role::Selected), t.fg);
  } else
    fill(at(Role::Selected), t.selectBg, t.selectFg);
  fill(at(Role::Input), t.inputBg, t.fg);
  fill(at(Role::Cursor), t.cursorBg, t.cursorFg);
  fill(at(Role::Drawer), t.drawerBg, t.fg);
  lv_style_set_border_color(at(Role::DrawerEdge), color(t.drawerEdge));
  lv_style_set_border_width(at(Role::DrawerEdge), 1);
  lv_style_set_border_side(at(Role::DrawerEdge), LV_BORDER_SIDE_LEFT);
  text(at(Role::Error), t.error);
  lv_style_set_bg_color(at(Role::Mention), color(t.mentionBg));
  lv_style_set_bg_opa(at(Role::Mention), LV_OPA_COVER);
  text(at(Role::Dim), t.dim);
  text(at(Role::Time), t.time);
  text(at(Role::Sys), t.sys);
  text(at(Role::Nick), t.nick);
  text(at(Role::SelfNick), t.selfNick);
}

}  // namespace

lv_color_t color(uint16_t c) {
  const uint8_t r = (c >> 11) & 0x1F, g = (c >> 5) & 0x3F, b = c & 0x1F;
  return lv_color_make(uint8_t((r << 3) | (r >> 2)), uint8_t((g << 2) | (g >> 4)), uint8_t((b << 3) | (b >> 2)));
}

void applyTheme(const rc::Theme& t) {
  current = t;
  applied = true;
  ++serial;
  build(current);
  lv_obj_report_style_change(nullptr);
}

lv_style_t* style(Role r) {
  ensure();
  if (!ready) build(current);
  return at(r);
}

void addStyle(lv_obj_t* obj, Role r, lv_style_selector_t sel) { lv_obj_add_style(obj, style(r), sel); }

const rc::Theme& currentTheme() {
  ensure();
  return current;
}

unsigned themeSerial() { return serial; }

}  // namespace lvui
