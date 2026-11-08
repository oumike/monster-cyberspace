#include <unity.h>

#include <lvgl.h>

#include <cstring>

#include "lvui/lv_init.h"
#include "lvui/styles.h"
#include "ui/theme.h"

using lvui::Role;

namespace {

uint32_t now = 0;
uint32_t nowMs() { return now; }
uint16_t shown[16 * 16];
int flushes = 0;
void copyFlush(lv_display_t* d, const lv_area_t*, uint8_t* px) {
  memcpy(shown, px, sizeof(shown));
  ++flushes;
  lv_display_flush_ready(d);
}

alignas(LV_DRAW_BUF_ALIGN) uint8_t buf[16 * 16 * 2];
lv_display_t* disp = nullptr;

uint16_t rgb565(lv_color_t c) { return lv_color_to_u16(c); }

}  // namespace

void setUp() {
  lvui::init();
  lvui::setTickSource(nowMs);
  disp = lv_display_create(16, 16);
  lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
  lv_display_set_buffers(disp, buf, nullptr, sizeof(buf), LV_DISPLAY_RENDER_MODE_FULL);
  lv_display_set_flush_cb(disp, copyFlush);
}

void tearDown() {
  lv_display_delete(disp);
  disp = nullptr;
}

void test_every_theme_maps_its_colours() {
  for (int i = 0; i < rc::kThemeCount; ++i) {
    const rc::Theme& t = rc::theme(rc::themeFromIndex(i));
    lvui::applyTheme(t);
    TEST_ASSERT_EQUAL_HEX16(t.bg, lvui::currentTheme().bg);
    TEST_ASSERT_EQUAL_HEX16(t.selectBg, lvui::currentTheme().selectBg);
    TEST_ASSERT_EQUAL(t.outlined, lvui::currentTheme().outlined);
    lv_obj_t* scr = lv_screen_active();
    lv_obj_t* screen = lv_obj_create(scr);
    lv_obj_t* item = lv_label_create(scr);
    lv_obj_t* sel = lv_obj_create(scr);
    lvui::addStyle(screen, Role::Screen);
    lvui::addStyle(item, Role::Item);
    lvui::addStyle(sel, Role::Selected);
    TEST_ASSERT_EQUAL_HEX16_MESSAGE(t.bg, rgb565(lv_obj_get_style_bg_color(screen, LV_PART_MAIN)), "screen bg");
    TEST_ASSERT_EQUAL_MESSAGE(LV_OPA_COVER, lv_obj_get_style_bg_opa(screen, LV_PART_MAIN), "screen opa");
    TEST_ASSERT_EQUAL_HEX16_MESSAGE(t.fg, rgb565(lv_obj_get_style_text_color(item, LV_PART_MAIN)), "item fg");
    if (t.outlined) {
      TEST_ASSERT_EQUAL_MESSAGE(LV_OPA_TRANSP, lv_obj_get_style_bg_opa(sel, LV_PART_MAIN), "outlined no fill");
      TEST_ASSERT_EQUAL_MESSAGE(1, lv_obj_get_style_border_width(sel, LV_PART_MAIN), "outlined border");
      TEST_ASSERT_EQUAL_HEX16(t.selectBg, rgb565(lv_obj_get_style_border_color(sel, LV_PART_MAIN)));
      TEST_ASSERT_EQUAL_HEX16(t.fg, rgb565(lv_obj_get_style_text_color(sel, LV_PART_MAIN)));
    } else {
      TEST_ASSERT_EQUAL_MESSAGE(LV_OPA_COVER, lv_obj_get_style_bg_opa(sel, LV_PART_MAIN), "selected fill");
      TEST_ASSERT_EQUAL_HEX16_MESSAGE(t.selectBg, rgb565(lv_obj_get_style_bg_color(sel, LV_PART_MAIN)), "selected bg");
      TEST_ASSERT_EQUAL_HEX16(t.selectFg, rgb565(lv_obj_get_style_text_color(sel, LV_PART_MAIN)));
      TEST_ASSERT_EQUAL(0, lv_obj_get_style_border_width(sel, LV_PART_MAIN));
    }
    lv_obj_clean(scr);
  }
}

void test_paper_theme_outlines_focus() {
  const rc::Theme& t = rc::paperTheme();
  lvui::applyTheme(t);
  lv_obj_t* f = lv_obj_create(lv_screen_active());
  lvui::addStyle(f, Role::Focused);
  TEST_ASSERT_EQUAL(LV_OPA_TRANSP, lv_obj_get_style_bg_opa(f, LV_PART_MAIN));
  TEST_ASSERT_EQUAL(1, lv_obj_get_style_border_width(f, LV_PART_MAIN));
  TEST_ASSERT_EQUAL_HEX16(t.selectBg, rgb565(lv_obj_get_style_border_color(f, LV_PART_MAIN)));
  TEST_ASSERT_EQUAL_HEX16(t.fg, rgb565(lv_obj_get_style_text_color(f, LV_PART_MAIN)));
}

void test_role_pairs() {
  const rc::Theme& t = rc::theme(rc::ThemeId::Dark);
  lvui::applyTheme(t);
  struct Want { Role r; bool hasBg; uint16_t bg; uint16_t fg; };
  const Want wants[] = {
      {Role::Bar, true, t.barBg, t.barFg},       {Role::Hint, true, t.hintBg, t.hintFg},
      {Role::Input, true, t.inputBg, t.fg},      {Role::Cursor, true, t.cursorBg, t.cursorFg},
      {Role::Drawer, true, t.drawerBg, t.fg},    {Role::Error, false, 0, t.error},
      {Role::Dim, false, 0, t.dim},              {Role::Time, false, 0, t.time},
      {Role::Sys, false, 0, t.sys},              {Role::Nick, false, 0, t.nick},
      {Role::SelfNick, false, 0, t.selfNick},
  };
  for (const Want& w : wants) {
    lv_obj_t* o = lv_label_create(lv_screen_active());
    lvui::addStyle(o, w.r);
    TEST_ASSERT_EQUAL_HEX16(w.fg, rgb565(lv_obj_get_style_text_color(o, LV_PART_MAIN)));
    if (w.hasBg) {
      TEST_ASSERT_EQUAL(LV_OPA_COVER, lv_obj_get_style_bg_opa(o, LV_PART_MAIN));
      TEST_ASSERT_EQUAL_HEX16(w.bg, rgb565(lv_obj_get_style_bg_color(o, LV_PART_MAIN)));
    }
  }
  lv_obj_t* m = lv_obj_create(lv_screen_active());
  lvui::addStyle(m, Role::Mention);
  TEST_ASSERT_EQUAL_HEX16(t.mentionBg, rgb565(lv_obj_get_style_bg_color(m, LV_PART_MAIN)));
  lv_obj_t* e = lv_obj_create(lv_screen_active());
  lvui::addStyle(e, Role::DrawerEdge);
  TEST_ASSERT_EQUAL_HEX16(t.drawerEdge, rgb565(lv_obj_get_style_border_color(e, LV_PART_MAIN)));
  TEST_ASSERT_EQUAL(LV_BORDER_SIDE_LEFT, lv_obj_get_style_border_side(e, LV_PART_MAIN));
  // Focused, with a colour theme: today's cursor outline, no fill.
  lv_obj_t* f = lv_obj_create(lv_screen_active());
  lvui::addStyle(f, Role::Focused);
  TEST_ASSERT_EQUAL(LV_OPA_TRANSP, lv_obj_get_style_bg_opa(f, LV_PART_MAIN));
  TEST_ASSERT_EQUAL_HEX16(t.selectBg, rgb565(lv_obj_get_style_border_color(f, LV_PART_MAIN)));
}

void test_restyle_on_theme_change() {
  lvui::applyTheme(rc::theme(rc::ThemeId::Default));
  lv_obj_t* scr = lv_screen_active();
  lvui::addStyle(scr, Role::Screen);
  lv_obj_t* label = lv_label_create(scr);
  lvui::addStyle(label, Role::Item);
  lv_label_set_text(label, "x");
  now += 50;
  lv_timer_handler();
  TEST_ASSERT_EQUAL_HEX16(rc::defaultTheme().bg, shown[15 * 16 + 15]);
  const rc::Theme& c64 = rc::theme(rc::ThemeId::C64);
  TEST_ASSERT_NOT_EQUAL(c64.fg, rgb565(lv_obj_get_style_text_color(label, LV_PART_MAIN)));
  const int before = flushes;
  lvui::applyTheme(c64);
  now += 50;
  lv_timer_handler();
  TEST_ASSERT_EQUAL_HEX16(c64.fg, rgb565(lv_obj_get_style_text_color(label, LV_PART_MAIN)));
  // The screen is redrawn, in the new colours, with no other change.
  TEST_ASSERT_GREATER_THAN(before, flushes);
  TEST_ASSERT_EQUAL_HEX16(c64.bg, shown[15 * 16 + 15]);
  lv_obj_remove_style_all(scr);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_every_theme_maps_its_colours);
  RUN_TEST(test_paper_theme_outlines_focus);
  RUN_TEST(test_role_pairs);
  RUN_TEST(test_restyle_on_theme_change);
  return UNITY_END();
}
