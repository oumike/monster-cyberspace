#include <unity.h>

#include <lvgl.h>

#include <string>
#include <vector>

#include "lvui/display.h"
#include "lvui/input.h"
#include "lvui/lv_init.h"
#include "lvui/styles.h"
#include "lvui/views/banner.h"
#include "lvui/views/drawer_panel.h"
#include "lvui/views/entry_list_box.h"
#include "lvui/views/list_box.h"
#include "lvui/views/menu_overlay.h"
#include "lvui/views/side_nav_strip.h"
#include "lvui/views/top_bar.h"
#include "ui/nick_color.h"
#include "ui/theme.h"
#include "util/utf8.h"

using rc::Action;
using rc::ListId;

namespace {

uint32_t now = 0;
uint32_t nowMs() { return now; }
void noRows(const uint16_t*, int, int) {}

int16_t ptrX = 0, ptrY = 0;
bool ptrDown = false;
bool readPtr(int16_t& x, int16_t& y) {
  x = ptrX;
  y = ptrY;
  return ptrDown;
}

std::vector<rc::KeyEvent> keys;
void sink(const rc::KeyEvent& e) { keys.push_back(e); }

void step(uint32_t ms = 40) {
  now += ms;
  lv_timer_handler();
}

// A finger down at p for about holdMs, then up.
void press(lv_point_t p, uint32_t holdMs) {
  ptrX = static_cast<int16_t>(p.x);
  ptrY = static_cast<int16_t>(p.y);
  ptrDown = true;
  step();
  for (uint32_t t = 0; t < holdMs; t += 40) step();
  ptrDown = false;
  step();
  step();
}
void tap(lv_point_t p) { press(p, 0); }

lv_point_t centre(lv_obj_t* o) {
  lv_obj_update_layout(lv_screen_active());
  lv_obj_update_layout(lv_layer_top());
  lv_area_t a;
  lv_obj_get_coords(o, &a);
  return {(a.x1 + a.x2) / 2, (a.y1 + a.y2) / 2};
}

bool contains(lv_obj_t* o, lv_point_t p) {
  lv_area_t a;
  lv_obj_get_coords(o, &a);
  return p.x >= a.x1 && p.x <= a.x2 && p.y >= a.y1 && p.y <= a.y2;
}

const char* text(lv_obj_t* label) { return lv_label_get_text(label); }

uint16_t rgb565(lv_color_t c) { return lv_color_to_u16(c); }

void assertActivate(const rc::KeyEvent& e, int index, ListId list, Action then) {
  TEST_ASSERT_TRUE(e.action == Action::Activate);
  TEST_ASSERT_EQUAL(index, e.index);
  TEST_ASSERT_EQUAL(static_cast<int>(list), e.target);
  TEST_ASSERT_TRUE(e.then == then);
}

std::vector<rc::ListItem> threeItems() {
  return {rc::ListItem{U"Alpha", U""}, rc::ListItem{U"Beta", U"on"},
          rc::ListItem{U"Gamma", U"", rc::ItemTone::Error}};
}

rc::PresenceEntry person(const char* name) {
  rc::PresenceEntry p;
  p.username = name;
  p.online = true;
  return p;
}

}  // namespace

void setUp() {
  lvui::setTickSource(nowMs);
  lvui::init();
  static lv_display_t* disp = lvui::displayBegin({320, 240, noRows});
  (void)disp;
  lvui::pointerBegin(readPtr);
  lvui::setKeySink(sink);
  lvui::applyTheme(rc::defaultTheme());
  rc::setCursorShown(true);
  lv_obj_clean(lv_screen_active());
  lv_obj_clean(lv_layer_top());
  step();
  keys.clear();
}
void tearDown() {}

void test_list_box_mirrors_model() {
  rc::ListView lv;
  lv.setItems(threeItems());
  lv.setSelected(1);
  lvui::ListBox box;
  box.create(lv_screen_active());
  box.sync(lv);
  TEST_ASSERT_EQUAL(3, lv_obj_get_child_count(box.obj()));
  lv_obj_t* row1 = lv_obj_get_child(box.obj(), 1);
  TEST_ASSERT_TRUE(lv_obj_has_state(row1, LV_STATE_CHECKED));
  TEST_ASSERT_FALSE(lv_obj_has_state(lv_obj_get_child(box.obj(), 0), LV_STATE_CHECKED));
  const rc::Theme& t = rc::defaultTheme();
  TEST_ASSERT_EQUAL_HEX16(t.selectBg, rgb565(lv_obj_get_style_bg_color(row1, LV_PART_MAIN)));
  TEST_ASSERT_EQUAL_STRING("Beta", text(box.label(1)));
  TEST_ASSERT_EQUAL_STRING(" on ", text(box.right(1)));
  // An Error item keeps the error colour.
  TEST_ASSERT_EQUAL_HEX16(t.error, rgb565(lv_obj_get_style_text_color(box.label(2), LV_PART_MAIN)));

  // Unchanged shape: the same objects, the selection moved.
  lv.setSelected(2);
  box.sync(lv);
  TEST_ASSERT_EQUAL_PTR(row1, lv_obj_get_child(box.obj(), 1));
  TEST_ASSERT_FALSE(lv_obj_has_state(row1, LV_STATE_CHECKED));
  TEST_ASSERT_TRUE(lv_obj_has_state(lv_obj_get_child(box.obj(), 2), LV_STATE_CHECKED));

  // No cursor on keyless boards: nothing is marked.
  rc::setCursorShown(false);
  box.sync(lv);
  TEST_ASSERT_FALSE(lv_obj_has_state(lv_obj_get_child(box.obj(), 2), LV_STATE_CHECKED));
}

void test_list_box_scrolls_selection_into_view() {
  rc::ListView lv;
  std::vector<rc::ListItem> items;
  for (int i = 0; i < 30; ++i) items.push_back(rc::ListItem{U"row", U""});
  lv.setItems(items);
  lvui::ListBox box;
  box.create(lv_screen_active());
  lv_obj_set_size(box.obj(), 320, 64);
  box.sync(lv);
  lv.setSelected(20);
  box.sync(lv);
  lv_obj_update_layout(lv_screen_active());
  lv_area_t view, row;
  lv_obj_get_coords(box.obj(), &view);
  lv_obj_get_coords(lv_obj_get_child(box.obj(), 20), &row);
  TEST_ASSERT_TRUE(row.y1 >= view.y1 && row.y2 <= view.y2);
}

void test_list_tap_emits_activate() {
  rc::ListView lv;
  lv.setItems(threeItems());
  lvui::ListBox box;
  box.create(lv_screen_active());
  box.sync(lv);
  tap(centre(lv_obj_get_child(box.obj(), 2)));
  TEST_ASSERT_EQUAL(1, keys.size());
  assertActivate(keys[0], 2, ListId::Main, Action::Select);

  // A long press: the palette on that row, and no tap after it.
  keys.clear();
  press(centre(lv_obj_get_child(box.obj(), 1)), 600);
  TEST_ASSERT_EQUAL(1, keys.size());
  assertActivate(keys[0], 1, ListId::Main, Action::Palette);

  keys.clear();
  box.setListId(ListId::Drawer);
  tap(centre(lv_obj_get_child(box.obj(), 0)));
  TEST_ASSERT_EQUAL(1, keys.size());
  assertActivate(keys[0], 0, ListId::Drawer, Action::Select);
}

void test_overlay_captures_taps() {
  rc::ListView lv;
  std::vector<rc::ListItem> items;
  for (int i = 0; i < 6; ++i) items.push_back(rc::ListItem{U"under", U""});
  lv.setItems(items);
  lvui::ListBox under;
  under.create(lv_screen_active());
  under.sync(lv);

  rc::Overlay ov;
  ov.open(U"Menu", threeItems());
  lvui::MenuOverlay menu;
  menu.create();
  menu.setPlace(rc::OverlayPlace::Centre);
  menu.sync(ov);
  lv_obj_t* top = lv_obj_get_child(menu.list().obj(), 0);
  lv_point_t p = centre(top);
  // Put list row 2 under the overlay's row 0.
  lv_area_t row2;
  lv_obj_get_coords(lv_obj_get_child(under.obj(), 2), &row2);
  lv_obj_set_y(under.obj(), p.y - (row2.y1 + row2.y2) / 2);
  lv_obj_update_layout(lv_screen_active());
  TEST_ASSERT_TRUE(contains(lv_obj_get_child(under.obj(), 2), p));
  TEST_ASSERT_TRUE(contains(top, p));

  tap(p);
  TEST_ASSERT_EQUAL(1, keys.size());
  assertActivate(keys[0], 0, ListId::Menu, Action::Select);

  // Elsewhere on the screen, the backdrop: Back, never the list beneath.
  keys.clear();
  tap({2, 2});
  TEST_ASSERT_EQUAL(1, keys.size());
  TEST_ASSERT_TRUE(keys[0].action == Action::Back);

  // Closed: hidden, and the list beneath takes taps again.
  ov.close();
  menu.sync(ov);
  keys.clear();
  tap(p);
  TEST_ASSERT_EQUAL(1, keys.size());
  assertActivate(keys[0], 2, ListId::Main, Action::Select);
}

void test_overlay_title_and_rows() {
  rc::Overlay ov;
  ov.confirm(U"Leave room?", U"Leave");
  lvui::MenuOverlay menu;
  menu.create();
  menu.setPlace(rc::OverlayPlace::BottomRight);
  menu.sync(ov);
  TEST_ASSERT_EQUAL_STRING(" Leave room?", text(menu.title()));
  TEST_ASSERT_EQUAL(2, lv_obj_get_child_count(menu.list().obj()));
  TEST_ASSERT_EQUAL_STRING("Leave", text(menu.list().label(1)));
  TEST_ASSERT_TRUE(lv_obj_has_state(lv_obj_get_child(menu.list().obj(), 0), LV_STATE_CHECKED));
  // Bottom right, a column in from the edge, at the bottom: 30 columns wide.
  lv_obj_update_layout(lv_layer_top());
  lv_area_t a;
  lv_obj_get_coords(menu.box(), &a);
  TEST_ASSERT_EQUAL(30 * 8, a.x2 - a.x1 + 1);
  TEST_ASSERT_EQUAL(320 - 8 - 1, a.x2);
  TEST_ASSERT_EQUAL(3 * 16, a.y2 - a.y1 + 1);
  TEST_ASSERT_EQUAL(239, a.y2);
}

void test_top_bar_texts() {
  lvui::TopBar bar;
  bar.create(lv_screen_active());
  lvui::TopBarData d;
  d.title = U"Rooms";
  d.count = 3;
  d.conn = U'\u25CF';
  d.wifiBars = 2;
  d.batteryPct = 80;
  d.clock = "12:34";
  bar.sync(d);
  TEST_ASSERT_EQUAL_STRING(" Rooms 3", text(bar.title()));
  TEST_ASSERT_EQUAL_STRING("\u25CF \u2582\u2584\u00B7\u00B7 80% 12:34 ", text(bar.right()));
  TEST_ASSERT_TRUE(lv_obj_has_flag(bar.back(), LV_OBJ_FLAG_HIDDEN));
  TEST_ASSERT_TRUE(lv_obj_has_flag(bar.menu(), LV_OBJ_FLAG_HIDDEN));

  // Touch: ‹ at the left (Back, held Home) and ⋯ after the clock (Palette).
  d.count = -1;
  d.batteryPct = -1;
  d.touch.on = true;
  d.touch.menu = true;
  bar.sync(d);
  TEST_ASSERT_EQUAL_STRING("Rooms", text(bar.title()));
  TEST_ASSERT_EQUAL_STRING("\u25CF \u2582\u2584\u00B7\u00B7 12:34 \u22EF ", text(bar.right()));
  TEST_ASSERT_FALSE(lv_obj_has_flag(bar.back(), LV_OBJ_FLAG_HIDDEN));
  tap(centre(bar.back()));
  press(centre(bar.back()), 600);
  tap(centre(bar.menu()));
  TEST_ASSERT_EQUAL(3, keys.size());
  TEST_ASSERT_TRUE(keys[0].action == Action::Back);
  TEST_ASSERT_TRUE(keys[1].action == Action::OpenHome);
  TEST_ASSERT_TRUE(keys[2].action == Action::Palette);

  // A long title is cut with an ellipsis, a cell clear of the right side.
  d.title = std::u32string(60, U'x');
  d.touch = {};
  d.batteryPct = 80;
  bar.sync(d);
  const std::u32string cut = rc::utf8ToU32(text(bar.title()));
  TEST_ASSERT_TRUE(cut.back() == U'\u2026');
  const int rightCells = 17;  // "● ▂▄·· 80% 12:34 "
  TEST_ASSERT_EQUAL(40 - rightCells - 1, static_cast<int>(cut.size()));
}

void test_side_nav_emits() {
  lvui::SideNavStrip nav;
  nav.create(lv_screen_active());
  nav.sync();
  tap(centre(nav.button(0)));
  tap(centre(nav.button(1)));
  tap(centre(nav.button(2)));
  TEST_ASSERT_EQUAL(3, keys.size());
  TEST_ASSERT_TRUE(keys[0].action == Action::Up);
  TEST_ASSERT_TRUE(keys[1].action == Action::Select);
  TEST_ASSERT_TRUE(keys[2].action == Action::Down);
  // About 18 px: three 8 px cells, at the right edge.
  lv_area_t a;
  lv_obj_get_coords(nav.obj(), &a);
  TEST_ASSERT_EQUAL(24, a.x2 - a.x1 + 1);
  TEST_ASSERT_EQUAL(319, a.x2);
}

int hides = 0;
void onHide() { ++hides; }

void test_banner_shows_and_hides() {
  rc::Banner b;
  lvui::Banner view;
  view.create(lv_screen_active());
  view.setOnHide(onHide);
  view.sync(b, 0);
  TEST_ASSERT_TRUE(lv_obj_has_flag(view.obj(), LV_OBJ_FLAG_HIDDEN));
  b.show(U"Saved", 0, 1000, true);
  view.sync(b, 10);
  TEST_ASSERT_FALSE(lv_obj_has_flag(view.obj(), LV_OBJ_FLAG_HIDDEN));
  TEST_ASSERT_EQUAL_STRING(" Saved", text(view.label()));
  const rc::Theme& t = rc::defaultTheme();
  TEST_ASSERT_EQUAL_HEX16(t.error, rgb565(lv_obj_get_style_bg_color(view.obj(), LV_PART_MAIN)));
  tap(centre(view.obj()));
  TEST_ASSERT_EQUAL(1, hides);
  TEST_ASSERT_EQUAL(0, keys.size());
  view.sync(b, 2000);
  TEST_ASSERT_TRUE(lv_obj_has_flag(view.obj(), LV_OBJ_FLAG_HIDDEN));
}

void test_entry_list_rows_and_tap() {
  const rc::Theme& t = rc::defaultTheme();
  rc::EntryList el;
  el.setRows({rc::FeedRow{U"ann \u00B7 3h", U"Hello", t.fg, t.dim},
              rc::FeedRow{U"bob \u00B7 1d", U"World", t.fg, t.dim}});
  el.setSelected(1);
  lvui::EntryListBox box;
  box.create(lv_screen_active());
  box.sync(el);
  TEST_ASSERT_EQUAL(2, lv_obj_get_child_count(box.obj()));
  TEST_ASSERT_EQUAL_STRING("World", text(box.line2(1)));
  TEST_ASSERT_EQUAL_HEX16(t.dim, rgb565(lv_obj_get_style_text_color(box.line2(1), LV_PART_MAIN)));
  TEST_ASSERT_TRUE(lv_obj_has_state(lv_obj_get_child(box.obj(), 1), LV_STATE_CHECKED));
  tap(centre(box.line2(0)));
  TEST_ASSERT_EQUAL(1, keys.size());
  assertActivate(keys[0], 0, ListId::Main, Action::Select);
}

void test_drawer_rows_and_close() {
  rc::Drawer d;
  d.setSelf("me");
  d.setPeople({person("ann"), person("me")}, 0, 60000);
  lvui::DrawerPanel panel;
  panel.create(lv_screen_active());
  panel.setDocked(true);
  panel.sync(d);
  TEST_ASSERT_EQUAL_STRING("here \u00B7 2", text(panel.header()));
  TEST_ASSERT_EQUAL_STRING("me", text(panel.name(1)));
  const rc::Theme& t = rc::defaultTheme();
  TEST_ASSERT_EQUAL_HEX16(rc::nickColor(t, "me", true),
                          rgb565(lv_obj_get_style_text_color(panel.name(1), LV_PART_MAIN)));
  tap(centre(panel.name(1)));
  tap(centre(panel.close()));
  TEST_ASSERT_EQUAL(2, keys.size());
  assertActivate(keys[0], 1, ListId::Drawer, Action::Select);
  assertActivate(keys[1], rc::Drawer::kCloseIndex, ListId::Drawer, Action::None);
  // Docked: a fifth of 40 columns, at least 15, at the right edge.
  lv_area_t a;
  lv_obj_get_coords(panel.obj(), &a);
  TEST_ASSERT_EQUAL(15 * 8, a.x2 - a.x1 + 1);
  TEST_ASSERT_EQUAL(319, a.x2);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_list_box_mirrors_model);
  RUN_TEST(test_list_box_scrolls_selection_into_view);
  RUN_TEST(test_list_tap_emits_activate);
  RUN_TEST(test_overlay_captures_taps);
  RUN_TEST(test_overlay_title_and_rows);
  RUN_TEST(test_top_bar_texts);
  RUN_TEST(test_side_nav_emits);
  RUN_TEST(test_banner_shows_and_hides);
  RUN_TEST(test_entry_list_rows_and_tap);
  RUN_TEST(test_drawer_rows_and_close);
  return UNITY_END();
}
