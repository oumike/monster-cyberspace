#include <unity.h>

#include <lvgl.h>

#include <string>
#include <vector>

#include "lvui/cell_font.h"
#include "lvui/display.h"
#include "lvui/host.h"
#include "lvui/input.h"
#include "lvui/lv_init.h"
#include "lvui/styles.h"
#include "lvui/views/banner.h"
#include "lvui/views/cell_text.h"
#include "lvui/views/drawer_panel.h"
#include "lvui/views/entry_list_box.h"
#include "lvui/views/list_box.h"
#include "lvui/views/menu_overlay.h"
#include "lvui/views/side_nav_strip.h"
#include "lvui/views/top_bar.h"
#include "input/gestures.h"
#include "input/touch_resolve.h"
#include "ui/grid.h"
#include "ui/tap_map.h"
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
// Set by a test: an Activate on liveId selects there, as FeedListScreen::onKey
// does on Main.
rc::EntryList* liveModel = nullptr;
rc::ListView* liveList = nullptr;
rc::Drawer* liveDrawer = nullptr;
ListId liveId = ListId::Main;
void sink(const rc::KeyEvent& e) {
  keys.push_back(e);
  if (e.action != Action::Activate || e.target != static_cast<uint8_t>(liveId)) return;
  if (liveModel) liveModel->select(e.index);
  if (liveList) liveList->select(e.index);
  if (liveDrawer) liveDrawer->select(e.index);
}

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

// One touch sample: finger down or up at (x, y), at ms.
struct Sample {
  bool down;
  int16_t x, y;
  uint32_t ms;
};

// A finger down at `from`, moved dy px in `moves` even steps stepMs apart, then up.
std::vector<Sample> fingerPath(lv_point_t from, int dy, int moves, uint32_t stepMs) {
  std::vector<Sample> out;
  uint32_t t = now;
  auto at = [&](bool down, int y, uint32_t dt) {
    t += dt;
    out.push_back(Sample{down, static_cast<int16_t>(from.x), static_cast<int16_t>(y), t});
  };
  at(true, from.y, 40);
  for (int i = 1; i <= moves; ++i) at(true, from.y + dy * i / moves, stepMs);
  at(false, from.y + dy, 40);
  at(false, from.y + dy, 40);
  return out;
}

// Plays samples through LVGL, syncing the box to the live model after each.
template <typename Box, typename Model>
void play(const std::vector<Sample>& ss, Box& box, const Model& m) {
  for (const Sample& s : ss) {
    ptrX = s.x;
    ptrY = s.y;
    ptrDown = s.down;
    now = s.ms;
    lv_timer_handler();
    box.sync(m);
  }
}

// play, through a pointer that reports where the finger lifts (lvui's
// reports no point then, so through it a flick is a drag or nothing).
template <typename Box, typename Model>
void playLifting(const std::vector<Sample>& ss, Box& box, const Model& m) {
  lv_indev_t* lvuiPointer = lv_indev_get_next(nullptr);
  lv_indev_enable(lvuiPointer, false);
  lv_indev_t* lift = lv_indev_create();
  lv_indev_set_type(lift, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(lift, [](lv_indev_t*, lv_indev_data_t* d) {
    d->point.x = ptrX;
    d->point.y = ptrY;
    d->state = ptrDown ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
  });
  play(ss, box, m);
  lv_indev_delete(lift);
  lv_indev_enable(lvuiPointer, true);
}

// The keys today's grid makes for the same samples: el drawn over cell rows
// [top, bottom) of a 40x15 grid of 8x16 cells, its tap map resolved by
// rc::TouchResolver, each Activate applied and the list redrawn.
void drawModel(const rc::EntryList& el, rc::Grid& g, int top, int bottom) {
  el.render(g, top, bottom, rc::defaultTheme());
}
void drawModel(const rc::ListView& lv, rc::Grid& g, int top, int bottom) {
  lv.render(g, top, bottom, 0, g.cols(), rc::defaultTheme());
}
void drawModel(const rc::Drawer& d, rc::Grid& g, int top, int bottom) { d.render(g, top, bottom, rc::defaultTheme()); }

template <typename Model>
std::vector<rc::KeyEvent> gridKeys(Model el, int top, int bottom, const std::vector<Sample>& ss) {
  rc::TapMap map;
  rc::Gestures gs;
  gs.setWidth(320);
  rc::TouchResolver tr;
  auto render = [&] {
    rc::Grid g(40, 15);
    map.clear();
    map.setOrigin(rc::CellSize{8, 16}, 0);
    rc::setActiveTaps(&map);
    drawModel(el, g, top, bottom);
    rc::setActiveTaps(nullptr);
  };
  render();
  std::vector<rc::KeyEvent> out;
  for (const Sample& s : ss) {
    rc::Gesture g;
    if (!gs.feed(s.down, s.x, s.y, s.ms, g)) continue;
    rc::TouchOutput o;
    tr.resolve(g, map, o);
    for (const rc::KeyEvent& k : o.keys) {
      out.push_back(k);
      if (k.action == Action::Activate && k.target == static_cast<uint8_t>(liveId)) el.select(k.index);
    }
    render();
  }
  return out;
}

void assertSameKeys(const std::vector<rc::KeyEvent>& want, const std::vector<rc::KeyEvent>& got) {
  TEST_ASSERT_EQUAL(want.size(), got.size());
  for (size_t i = 0; i < want.size(); ++i) {
    TEST_ASSERT_TRUE(want[i].action == got[i].action);
    TEST_ASSERT_EQUAL(want[i].index, got[i].index);
    TEST_ASSERT_EQUAL(want[i].target, got[i].target);
    TEST_ASSERT_TRUE(want[i].then == got[i].then);
  }
}

std::vector<rc::FeedRow> tenEntries() {
  const rc::Theme& t = rc::defaultTheme();
  std::vector<rc::FeedRow> rows;
  for (int i = 0; i < 10; ++i) {
    std::u32string n = U"e" + rc::utf8ToU32(std::to_string(i));
    rows.push_back(rc::FeedRow{n + U" \u00B7 1h", n + U" body", t.fg, t.dim});
  }
  return rows;
}

// A finger down at from, still for stillMs, moved by (dx, dy) in `moves`
// even steps stepMs apart, still again for endMs, then up.
std::vector<Sample> movePath(lv_point_t from, int dx, int dy, int moves, uint32_t stepMs, uint32_t stillMs = 0,
                             uint32_t endMs = 0) {
  std::vector<Sample> out;
  uint32_t t = now;
  int x = from.x, y = from.y;
  auto at = [&](bool down, uint32_t dt) {
    t += dt;
    out.push_back(Sample{down, static_cast<int16_t>(x), static_cast<int16_t>(y), t});
  };
  at(true, 40);
  for (uint32_t s = 40; s <= stillMs; s += 40) at(true, 40);
  for (int i = 1; i <= moves; ++i) {
    x = from.x + dx * i / moves;
    y = from.y + dy * i / moves;
    at(true, stepMs);
  }
  for (uint32_t s = 40; s <= endMs; s += 40) at(true, 40);
  at(false, 40);
  at(false, 40);
  return out;
}

void playSamples(const std::vector<Sample>& ss) {
  for (const Sample& s : ss) {
    ptrX = s.x;
    ptrY = s.y;
    ptrDown = s.down;
    now = s.ms;
    lv_timer_handler();
  }
}

// The keys today's grid makes for the same samples on a Fixed target over
// o's area (tap, and hold for a long press; None: as the tap), through
// rc::Gestures and rc::TouchResolver.
std::vector<rc::KeyEvent> gridFixedKeys(lv_obj_t* o, Action tapAct, Action holdAct, const std::vector<Sample>& ss,
                                        bool* edge = nullptr) {
  lv_obj_update_layout(lv_screen_active());
  lv_obj_update_layout(lv_layer_top());
  lv_area_t a;
  lv_obj_get_coords(o, &a);
  rc::TapMap map;
  map.clear();
  map.setOrigin(rc::CellSize{1, 1}, 0);
  rc::Target t;
  t.kind = rc::TargetKind::Fixed;
  t.tap.action = tapAct;
  t.hold.action = holdAct;
  map.addCells(a.x1, a.y1, a.x2, a.y2, t);
  rc::Gestures gs;
  gs.setWidth(320);
  rc::TouchResolver tr;
  std::vector<rc::KeyEvent> out;
  if (edge) *edge = false;
  for (const Sample& s : ss) {
    rc::Gesture g;
    if (!gs.feed(s.down, s.x, s.y, s.ms, g)) continue;
    // Edge swipes are ui_task's, not a view's.
    if (g.kind == rc::GestureKind::EdgeBack || g.kind == rc::GestureKind::EdgeOptions) {
      if (edge) *edge = true;
      continue;
    }
    rc::TouchOutput r;
    tr.resolve(g, map, r);
    out.insert(out.end(), r.keys.begin(), r.keys.end());
  }
  return out;
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
  // The label rows; the painted ones are checked against them below.
  lvui::ListBox::setPainted(false);
  lv_obj_clean(lv_screen_active());
  lv_obj_clean(lv_layer_top());
  step();
  keys.clear();
  liveModel = nullptr;
  liveList = nullptr;
  liveDrawer = nullptr;
  liveId = ListId::Main;
}
void tearDown() {}


std::vector<rc::ListItem> numberedItems(int n) {
  std::vector<rc::ListItem> items;
  for (int i = 0; i < n; ++i) items.push_back(rc::ListItem{U"row " + rc::utf8ToU32(std::to_string(i)), U""});
  return items;
}

// The centre of row k as shown, painted or not.
lv_point_t rowCentre(const lvui::ListBox& box, int k) {
  lv_obj_update_layout(lv_screen_active());
  lv_obj_update_layout(lv_layer_top());
  const lv_area_t a = box.rowArea(k);
  return {(a.x1 + a.x2) / 2, (a.y1 + a.y2) / 2};
}

bool inArea(const lv_area_t& a, lv_point_t p) { return p.x >= a.x1 && p.x <= a.x2 && p.y >= a.y1 && p.y <= a.y2; }

// Cell rows [1, 14) of the 40x15 screen, as a screen places a list.
void placeListBox(lvui::ListBox& box) { lvui::place(box.obj(), 0, 16, 320, 13 * 16); }

void test_list_box_mirrors_model() {
  rc::ListView lv;
  lv.setItems(threeItems());
  lv.setSelected(1);
  lvui::ListBox box;
  box.create(lv_screen_active());
  placeListBox(box);
  box.sync(lv);
  TEST_ASSERT_EQUAL(3, box.shownItems());
  lv_obj_t* row1 = box.row(1);
  TEST_ASSERT_TRUE(lv_obj_has_state(row1, LV_STATE_CHECKED));
  TEST_ASSERT_FALSE(lv_obj_has_state(box.row(0), LV_STATE_CHECKED));
  const rc::Theme& t = rc::defaultTheme();
  // Outlined, not filled: the row keeps its own colours (list_view.cpp).
  TEST_ASSERT_EQUAL(LV_OPA_TRANSP, lv_obj_get_style_bg_opa(row1, LV_PART_MAIN));
  TEST_ASSERT_EQUAL(1, lv_obj_get_style_border_width(row1, LV_PART_MAIN));
  TEST_ASSERT_EQUAL_HEX16(t.selectBg, rgb565(lv_obj_get_style_border_color(row1, LV_PART_MAIN)));
  TEST_ASSERT_EQUAL_STRING("Beta", text(box.label(1)));
  TEST_ASSERT_EQUAL_STRING(" on ", text(box.right(1)));
  // An Error item keeps the error colour.
  TEST_ASSERT_EQUAL_HEX16(t.error, rgb565(lv_obj_get_style_text_color(box.label(2), LV_PART_MAIN)));

  // Unchanged shape: the same objects, the selection moved.
  lv.setSelected(2);
  box.sync(lv);
  TEST_ASSERT_EQUAL_PTR(row1, box.row(1));
  TEST_ASSERT_FALSE(lv_obj_has_state(row1, LV_STATE_CHECKED));
  TEST_ASSERT_TRUE(lv_obj_has_state(box.row(2), LV_STATE_CHECKED));
  // A selected Error row keeps the error colour.
  TEST_ASSERT_EQUAL_HEX16(t.error, rgb565(lv_obj_get_style_text_color(box.label(2), LV_PART_MAIN)));
  TEST_ASSERT_EQUAL(LV_OPA_TRANSP, lv_obj_get_style_bg_opa(box.row(2), LV_PART_MAIN));

  // No cursor on keyless boards: nothing is marked.
  rc::setCursorShown(false);
  box.sync(lv);
  TEST_ASSERT_FALSE(lv_obj_has_state(box.row(2), LV_STATE_CHECKED));
}

void test_list_box_scrolls_selection_into_view() {
  rc::ListView lv;
  lv.setItems(numberedItems(30));
  lvui::ListBox box;
  box.create(lv_screen_active());
  lvui::place(box.obj(), 0, 0, 320, 64);
  box.sync(lv);
  lv.setSelected(20);
  box.sync(lv);
  // Four rows, the selection on the last, nothing scrolled natively.
  TEST_ASSERT_EQUAL(17, box.firstShown());
  TEST_ASSERT_EQUAL_STRING("row 20", text(box.label(3)));
  TEST_ASSERT_TRUE(lv_obj_has_state(box.row(3), LV_STATE_CHECKED));
  TEST_ASSERT_EQUAL(0, lv_obj_get_scroll_y(box.obj()));
  lv_obj_update_layout(lv_screen_active());
  // Each row on its cell, the checked one's outline included, its text unmoved.
  for (int k = 0; k < 4; ++k) {
    lv_area_t row, label;
    lv_obj_get_coords(box.row(k), &row);
    lv_obj_get_coords(box.label(k), &label);
    TEST_ASSERT_EQUAL(k * 16, row.y1);
    TEST_ASSERT_EQUAL(k * 16 + 15, row.y2);
    TEST_ASSERT_EQUAL(k * 16, label.y1);
    TEST_ASSERT_EQUAL(2 * 8, label.x1);
  }
}

void test_list_tap_emits_activate() {
  rc::ListView lv;
  lv.setItems(threeItems());
  lvui::ListBox box;
  box.create(lv_screen_active());
  placeListBox(box);
  box.sync(lv);
  tap(rowCentre(box, 2));
  TEST_ASSERT_EQUAL(1, keys.size());
  assertActivate(keys[0], 2, ListId::Main, Action::Select);

  // A long press: the palette on that row, and no tap after it.
  keys.clear();
  press(rowCentre(box, 1), 600);
  TEST_ASSERT_EQUAL(1, keys.size());
  assertActivate(keys[0], 1, ListId::Main, Action::Palette);

  // The list id set after create names later keys.
  keys.clear();
  box.setListId(ListId::Drawer);
  tap(rowCentre(box, 0));
  TEST_ASSERT_EQUAL(1, keys.size());
  assertActivate(keys[0], 0, ListId::Drawer, Action::Select);

  // Below the rows: a screen list does nothing (the grid has no target there).
  keys.clear();
  tap({40, 16 + 8 * 16 + 8});
  TEST_ASSERT_EQUAL(0, keys.size());
  // Unless it misses back, as an overlay's list does over its backdrop.
  box.setMissBack(true);
  tap({40, 16 + 8 * 16 + 8});
  TEST_ASSERT_EQUAL(1, keys.size());
  TEST_ASSERT_TRUE(keys[0].action == Action::Back);
}

void test_overlay_captures_taps() {
  rc::ListView lv;
  std::vector<rc::ListItem> items;
  for (int i = 0; i < 6; ++i) items.push_back(rc::ListItem{U"under", U""});
  lv.setItems(items);
  lvui::ListBox under;
  under.create(lv_screen_active());
  placeListBox(under);
  under.sync(lv);

  rc::Overlay ov;
  ov.open(U"Menu", threeItems());
  lvui::MenuOverlay menu;
  menu.create();
  menu.setPlace(rc::OverlayPlace::Centre);
  menu.sync(ov);
  lv_point_t p = rowCentre(menu.list(), 0);
  // Put list row 2 under the overlay's row 0.
  const lv_area_t row2 = under.rowArea(2);
  lv_obj_set_y(under.obj(), lv_obj_get_y(under.obj()) + p.y - (row2.y1 + row2.y2) / 2);
  lv_obj_update_layout(lv_screen_active());
  TEST_ASSERT_TRUE(inArea(under.rowArea(2), p));
  TEST_ASSERT_TRUE(inArea(menu.list().rowArea(0), p));

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
  TEST_ASSERT_EQUAL(2, menu.list().shownItems());
  TEST_ASSERT_EQUAL_STRING("Leave", text(menu.list().label(1)));
  TEST_ASSERT_TRUE(lv_obj_has_state(menu.list().row(0), LV_STATE_CHECKED));
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
  // Without touch the ‹ glyph and the hit boxes are never made.
  TEST_ASSERT_NULL(bar.back());
  TEST_ASSERT_NULL(bar.menu());

  // Touch: ‹ at the left (Back, held Home) and ⋯ after the clock (Palette).
  d.count = -1;
  d.batteryPct = -1;
  d.touch.on = true;
  d.touch.menu = true;
  bar.sync(d);
  TEST_ASSERT_EQUAL_STRING("Rooms", text(bar.title()));
  TEST_ASSERT_EQUAL_STRING("\u25CF \u2582\u2584\u00B7\u00B7 12:34 \u22EF ", text(bar.right()));
  TEST_ASSERT_FALSE(lv_obj_has_flag(bar.back(), LV_OBJ_FLAG_HIDDEN));
  lv_obj_update_layout(bar.obj());
  TEST_ASSERT_EQUAL_PTR(bar.title(), lv_obj_get_child(bar.obj(), 1));  // ‹ first
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
  TEST_ASSERT_TRUE(lv_obj_has_flag(bar.back(), LV_OBJ_FLAG_HIDDEN));
  TEST_ASSERT_TRUE(lv_obj_has_flag(bar.menu(), LV_OBJ_FLAG_HIDDEN));
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
  lvui::place(box.obj(), 0, 0, 320, 240);
  box.sync(el);
  TEST_ASSERT_EQUAL(2, box.shownItems());
  TEST_ASSERT_EQUAL_STRING("World", text(box.line2(1)));
  TEST_ASSERT_EQUAL_HEX16(t.dim, rgb565(lv_obj_get_style_text_color(box.line2(1), LV_PART_MAIN)));
  TEST_ASSERT_TRUE(lv_obj_has_state(box.row(1), LV_STATE_CHECKED));
  TEST_ASSERT_FALSE(lv_obj_has_state(box.row(0), LV_STATE_CHECKED));
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

// Cell rows [1, 14) of the 40x15 screen: 13 rows, six whole two-row entries.
void placeEntryBox(lvui::EntryListBox& box) { lvui::place(box.obj(), 0, 16, 320, 13 * 16); }

void test_entry_list_drag_steps_selection() {
  rc::EntryList el;
  el.setRows(tenEntries());
  lvui::EntryListBox box;
  box.create(lv_screen_active());
  placeEntryBox(box);
  box.sync(el);
  liveModel = &el;
  // Finger up three cells over the second entry: later entries, one
  // {Activate, idx, Main, then None} a cell, as the grid's wrapping list.
  const auto path = fingerPath({40, 16 + 3 * 16 + 8}, -3 * 16, 12, 60);
  const auto want = gridKeys(el, 1, 14, path);
  TEST_ASSERT_EQUAL(3, want.size());
  play(path, box, el);
  assertSameKeys(want, keys);
  TEST_ASSERT_EQUAL(3, el.selected());
  // And back down two.
  keys.clear();
  const auto back = fingerPath({40, 16 + 3 * 16 + 8}, 2 * 16, 8, 60);
  const auto wantBack = gridKeys(el, 1, 14, back);
  TEST_ASSERT_EQUAL(2, wantBack.size());
  play(back, box, el);
  assertSameKeys(wantBack, keys);
  TEST_ASSERT_EQUAL(1, el.selected());
}

void test_entry_list_swipe_pages() {
  rc::EntryList el;
  el.setRows(tenEntries());
  lvui::EntryListBox box;
  box.create(lv_screen_active());
  placeEntryBox(box);
  box.sync(el);
  liveModel = &el;
  // A quick flick up (the panel sees only the ends): a page (six entries
  // shown) less one.
  const auto path = fingerPath({40, 16 + 8 * 16 + 8}, -80, 0, 40);
  const auto want = gridKeys(el, 1, 14, path);
  TEST_ASSERT_EQUAL(1, want.size());
  TEST_ASSERT_EQUAL(5, want[0].index);
  playLifting(path, box, el);
  assertSameKeys(want, keys);
  TEST_ASSERT_EQUAL(5, el.selected());
  // And back: a flick down from entry 5 returns to the top.
  keys.clear();
  const auto down = fingerPath({40, 16 + 2 * 16 + 8}, 80, 0, 40);
  const auto wantDown = gridKeys(el, 1, 14, down);
  playLifting(down, box, el);
  TEST_ASSERT_EQUAL(1, wantDown.size());
  assertSameKeys(wantDown, keys);
  TEST_ASSERT_EQUAL(0, el.selected());
}

void test_entry_list_draws_its_window_without_scrolling() {
  rc::EntryList el;
  el.setRows(tenEntries());
  el.setSelected(8);
  lvui::EntryListBox box;
  box.create(lv_screen_active());
  placeEntryBox(box);
  box.sync(el);
  TEST_ASSERT_FALSE(lv_obj_has_flag(box.obj(), LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_update_layout(lv_screen_active());
  TEST_ASSERT_EQUAL(0, lv_obj_get_scroll_y(box.obj()));
  // Whole entries only: e3..e8, the selection at the bottom, as the grid shows them.
  TEST_ASSERT_EQUAL(6, box.shownItems());
  TEST_ASSERT_EQUAL_STRING("e3 body", text(box.line2(0)));
  TEST_ASSERT_EQUAL_STRING("e8 body", text(box.line2(5)));
  TEST_ASSERT_FALSE(lv_obj_has_flag(box.scrollBar().thumb(), LV_OBJ_FLAG_HIDDEN));
  // A drag scrolls nothing natively.
  play(fingerPath({40, 16 + 2 * 16 + 8}, 4 * 16, 16, 60), box, el);
  TEST_ASSERT_EQUAL(0, lv_obj_get_scroll_y(box.obj()));
}


void test_list_box_window_draws_list_view_rows() {
  rc::ListView lv;
  lv.setItems(numberedItems(30));
  lv.setSelected(20);
  lvui::ListBox box;
  box.create(lv_screen_active());
  placeListBox(box);
  box.sync(lv);
  TEST_ASSERT_FALSE(lv_obj_has_flag(box.obj(), LV_OBJ_FLAG_SCROLLABLE));
  // As ListView::render: the selection on the last of 13 rows, rows 8..20.
  TEST_ASSERT_EQUAL(13, box.shownItems());
  TEST_ASSERT_EQUAL_STRING("row 8", text(box.label(0)));
  TEST_ASSERT_EQUAL_STRING("row 20", text(box.label(12)));
  TEST_ASSERT_TRUE(lv_obj_has_state(lv_obj_get_parent(box.label(12)), LV_STATE_CHECKED));
  TEST_ASSERT_FALSE(lv_obj_has_flag(box.scrollBar().thumb(), LV_OBJ_FLAG_HIDDEN));
  // The last column holds the bar: the rows stop a cell short.
  lv_obj_update_layout(lv_screen_active());
  TEST_ASSERT_EQUAL(39 * 8, lv_obj_get_width(lv_obj_get_parent(box.label(0))));
  // A short list: no bar, the rows take the full width.
  lv.setItems(numberedItems(3));
  box.sync(lv);
  TEST_ASSERT_EQUAL(3, box.shownItems());
  TEST_ASSERT_TRUE(lv_obj_has_flag(box.scrollBar().thumb(), LV_OBJ_FLAG_HIDDEN));
  lv_obj_update_layout(lv_screen_active());
  TEST_ASSERT_EQUAL(40 * 8, lv_obj_get_width(lv_obj_get_parent(box.label(0))));
  tap(centre(box.label(2)));
  TEST_ASSERT_EQUAL(1, keys.size());
  assertActivate(keys[0], 2, ListId::Main, Action::Select);
}

void test_list_box_window_drag_steps_selection() {
  rc::ListView lv;
  lv.setItems(numberedItems(30));
  lvui::ListBox box;
  box.create(lv_screen_active());
  placeListBox(box);
  box.sync(lv);
  liveList = &lv;
  // Finger up three cells: the grid's wrapping list steps one Activate a cell.
  const auto path = fingerPath({40, 16 + 6 * 16 + 8}, -3 * 16, 12, 60);
  const auto want = gridKeys(lv, 1, 14, path);
  TEST_ASSERT_EQUAL(3, want.size());
  play(path, box, lv);
  assertSameKeys(want, keys);
  TEST_ASSERT_EQUAL(3, lv.selected());
  TEST_ASSERT_EQUAL(0, lv_obj_get_scroll_y(box.obj()));
  // A flick: a page (13 rows) less one.
  keys.clear();
  const auto flick = fingerPath({40, 16 + 10 * 16 + 8}, -80, 0, 40);
  const auto wantFlick = gridKeys(lv, 1, 14, flick);
  TEST_ASSERT_EQUAL(1, wantFlick.size());
  playLifting(flick, box, lv);
  assertSameKeys(wantFlick, keys);
  TEST_ASSERT_EQUAL(15, lv.selected());
}

void test_list_box_drag_names_its_list() {
  // A non-Main list (an overlay's Menu): the same keys as the grid's tap map
  // with that list id, tap, drag and flick.
  rc::ListView lv;
  lv.setItems(numberedItems(30));
  lv.setListId(ListId::Menu);
  lvui::ListBox box;
  box.create(lv_screen_active());
  box.setListId(ListId::Menu);
  placeListBox(box);
  box.sync(lv);
  liveList = &lv;
  liveId = ListId::Menu;
  const auto tapPath = fingerPath({40, 16 + 4 * 16 + 8}, 0, 0, 40);
  const auto wantTap = gridKeys(lv, 1, 14, tapPath);
  TEST_ASSERT_EQUAL(1, wantTap.size());
  play(tapPath, box, lv);
  assertSameKeys(wantTap, keys);
  assertActivate(keys[0], 4, ListId::Menu, Action::Select);
  TEST_ASSERT_EQUAL(4, lv.selected());

  keys.clear();
  const auto path = fingerPath({40, 16 + 6 * 16 + 8}, -3 * 16, 12, 60);
  const auto want = gridKeys(lv, 1, 14, path);
  TEST_ASSERT_EQUAL(3, want.size());
  play(path, box, lv);
  assertSameKeys(want, keys);
  TEST_ASSERT_EQUAL(7, lv.selected());

  // Back down past the top: a wrapping list clamps at 0 (no Up keys).
  keys.clear();
  const auto down = fingerPath({40, 16 + 2 * 16 + 8}, 10 * 16, 20, 60);
  const auto wantDown = gridKeys(lv, 1, 14, down);
  play(down, box, lv);
  assertSameKeys(wantDown, keys);
  for (const rc::KeyEvent& k : keys) TEST_ASSERT_TRUE(k.action == Action::Activate);
  TEST_ASSERT_EQUAL(0, lv.selected());

  keys.clear();
  const auto flick = fingerPath({40, 16 + 10 * 16 + 8}, -80, 0, 40);
  const auto wantFlick = gridKeys(lv, 1, 14, flick);
  TEST_ASSERT_EQUAL(1, wantFlick.size());
  playLifting(flick, box, lv);
  assertSameKeys(wantFlick, keys);
  TEST_ASSERT_EQUAL(static_cast<int>(ListId::Menu), keys[0].target);
}

void test_overlay_bar_column_is_backdrop() {
  // A long menu: its list's bar column is no row in the grid's tap map, so
  // the backdrop's Back takes it.
  rc::Overlay ov;
  ov.open(U"Menu", numberedItems(20));
  lvui::MenuOverlay menu;
  menu.create();
  menu.setPlace(rc::OverlayPlace::Centre);
  menu.sync(ov);
  TEST_ASSERT_FALSE(lv_obj_has_flag(menu.list().scrollBar().track(), LV_OBJ_FLAG_HIDDEN));
  lv_obj_update_layout(lv_layer_top());
  lv_area_t bar;
  lv_obj_get_coords(menu.list().obj(), &bar);
  tap({bar.x2 - 2, rowCentre(menu.list(), 0).y});
  TEST_ASSERT_EQUAL(1, keys.size());
  TEST_ASSERT_TRUE(keys[0].action == Action::Back);
  keys.clear();
  tap(rowCentre(menu.list(), 1));
  TEST_ASSERT_EQUAL(1, keys.size());
  assertActivate(keys[0], 1, ListId::Menu, Action::Select);
}

// The touch header as the grid's (rc::touchHeader): the status bar is px tall
// in the big font and its taps are the same; the rows below it are as many,
// and where, as gridDims and rowSpans put them; a menu's backdrop takes the
// strip the bar reaches up over.
void test_touch_header_rows_match_grid() {
  const rc::TextSize ts = rc::TextSize::Comfortable;
  const lvui::Header hd = lvui::touchHeader(ts);
  TEST_ASSERT_EQUAL(rc::touchHeader(320, ts).px, hd.px);
  TEST_ASSERT_EQUAL_PTR(lvui::font(ts, true), hd.font);
  lvui::setHeader(hd);
  lvui::ScreenHost::setFreeProbe([]() -> size_t { return 1 << 20; });
  static lvui::CellText cells;
  static lvui::HintBar hint;
  static lvui::TopBar bar;
  static lvui::MenuOverlay menu;
  lvui::ScreenHost host;
  TEST_ASSERT_TRUE(host.show([](lv_obj_t* root) {
    cells.create(root);
    hint.create(root);
    bar.create(root);
    menu.create(root);
  }));

  int cols = 0, rows = 0;
  rc::gridDims(320, 240, ts, cols, rows, hd.px);
  rc::Grid g(cols, rows);
  g.setHeader(rc::touchHeader(320, ts));
  const rc::CellSize cs = rc::cellSize(ts);
  const std::vector<rc::RowSpan> spans = rc::rowSpans(g, cs.h, rc::bigCellSize(ts).h);
  lv_obj_update_layout(lv_screen_active());
  TEST_ASSERT_EQUAL(cols, cells.cols());
  TEST_ASSERT_EQUAL(rows, cells.rows());
  cells.begin();
  cells.line(0, 1, U"row one", 0xFFFF);
  cells.end();
  hint.sync(U"hint");
  lvui::TopBarData d;
  d.title = U"Rooms";
  d.touch.on = true;
  d.touch.menu = true;
  bar.sync(d);
  lv_obj_update_layout(lv_screen_active());
  lv_area_t a;
  lv_obj_get_coords(cells.label(0), &a);
  TEST_ASSERT_EQUAL(spans[1].y, a.y1);
  lv_obj_get_coords(hint.obj(), &a);
  TEST_ASSERT_EQUAL(spans.back().y, a.y1);
  lv_obj_get_coords(bar.obj(), &a);
  TEST_ASSERT_EQUAL(0, a.y1);
  TEST_ASSERT_EQUAL(hd.px, a.y2 - a.y1 + 1);
  TEST_ASSERT_EQUAL_PTR(hd.font, lv_obj_get_style_text_font(bar.title(), LV_PART_MAIN));

  // The header's tap targets: 4 big cells at each end, the whole bar tall.
  const int bigW = rc::bigCellSize(ts).w;
  tap({2, 1});
  tap({bigW * 4 - 2, hd.px - 2});
  tap({(320 / bigW - 1) * bigW, 1});
  TEST_ASSERT_EQUAL(3, keys.size());
  TEST_ASSERT_TRUE(keys[0].action == Action::Back);
  TEST_ASSERT_TRUE(keys[1].action == Action::Back);
  TEST_ASSERT_TRUE(keys[2].action == Action::Palette);
  keys.clear();

  rc::Overlay ov;
  ov.open(U"Menu", threeItems());
  menu.setPlace(rc::OverlayPlace::Centre);
  menu.sync(ov);
  // Over ⋯, in the strip: the backdrop's Back, not the bar's Palette.
  tap({(320 / bigW - 1) * bigW, 1});
  TEST_ASSERT_EQUAL(1, keys.size());
  TEST_ASSERT_TRUE(keys[0].action == Action::Back);

  lvui::setHeader(lvui::Header{});
  lv_obj_delete(host.root());
}

// The whole frame, drawn now.
std::vector<uint16_t> shot() {
  lv_obj_invalidate(lv_screen_active());
  lv_obj_invalidate(lv_layer_top());
  lv_refr_now(nullptr);
  const uint16_t* f = lvui::frame();
  return std::vector<uint16_t>(f, f + 320 * 240);
}

int frameDiff(const std::vector<uint16_t>& a, const std::vector<uint16_t>& b) {
  int d = 0;
  for (size_t i = 0; i < a.size(); ++i) d += a[i] != b[i];
  return d;
}

// Lists drawn painted and as label rows: the same pixels.
void test_list_box_painted_matches_labels() {
  std::vector<rc::ListItem> odd = threeItems();
  odd.push_back(rc::ListItem{U"Marked", U"12", rc::ItemTone::Normal, true});
  odd.push_back(rc::ListItem{U"", U"right only"});
  odd.push_back(rc::ListItem{U"a label far too long for its row, cut where a word would wrap it here",
                             U"and right"});
  odd.push_back(rc::ListItem{U"wide \U0001F600 and \u2022", U"\u2713", rc::ItemTone::Error, true});
  odd.push_back(rc::ListItem{U"short", U"a right text longer than the whole row of forty cells, cut"});
  struct Case {
    std::vector<rc::ListItem> items;
    int selected;
    bool cursor;
    int h;
  };
  const Case cases[] = {{threeItems(), 1, true, 13}, {threeItems(), 2, true, 13}, {numberedItems(30), 20, true, 13},
                        {odd, 3, true, 13},          {odd, 6, true, 4},           {odd, 0, false, 13}};
  int i = 0;
  for (const Case& c : cases) {
    std::vector<uint16_t> want;
    for (bool painted : {false, true}) {
      lv_obj_clean(lv_screen_active());
      rc::setCursorShown(c.cursor);
      lvui::ListBox::setPainted(painted);
      rc::ListView lv;
      lv.setItems(c.items);
      lv.setSelected(c.selected);
      lvui::ListBox box;
      box.create(lv_screen_active());
      lvui::place(box.obj(), 0, 16, 320, c.h * 16);
      box.sync(lv);
      TEST_ASSERT_EQUAL(painted, box.painted());
      const std::vector<uint16_t> got = shot();
      if (!painted) {
        want = got;
        continue;
      }
      char msg[32];
      snprintf(msg, sizeof msg, "case %d", i);
      TEST_ASSERT_EQUAL_MESSAGE(0, frameDiff(got, want), msg);
      // The selection moved, an item changed and the list shrank: drawn again,
      // still the same as label rows drawn that way from the start.
      lv.setSelected((c.selected + 1) % lv.size());
      std::vector<rc::ListItem> next = c.items;
      next[0].label += U" changed";
      next.pop_back();
      lv.setItems(next);
      lv.setSelected((c.selected + 1) % lv.size());
      box.sync(lv);
      lv_refr_now(nullptr);
      const uint16_t* f = lvui::frame();
      const std::vector<uint16_t> partial(f, f + 320 * 240);
      lv_obj_clean(lv_screen_active());
      lvui::ListBox::setPainted(false);
      lvui::ListBox ref;
      ref.create(lv_screen_active());
      lvui::place(ref.obj(), 0, 16, 320, c.h * 16);
      ref.sync(lv);
      snprintf(msg, sizeof msg, "case %d changed", i);
      TEST_ASSERT_EQUAL_MESSAGE(0, frameDiff(partial, shot()), msg);
    }
    ++i;
  }
  rc::setCursorShown(true);
}

// A menu overlay painted: the same pixels as label rows.
void test_menu_overlay_painted_matches_labels() {
  std::vector<uint16_t> want;
  for (bool painted : {false, true}) {
    lv_obj_clean(lv_screen_active());
    lv_obj_clean(lv_layer_top());
    lvui::ListBox::setPainted(painted);
    rc::Overlay ov;
    ov.open(U"Menu", numberedItems(20));
    lvui::MenuOverlay menu;
    menu.create();
    menu.setPlace(rc::OverlayPlace::Centre);
    menu.sync(ov);
    const std::vector<uint16_t> got = shot();
    if (!painted)
      want = got;
    else
      TEST_ASSERT_EQUAL(0, frameDiff(got, want));
  }
}

void test_list_box_painted_has_no_row_objects() {
  lvui::ListBox::setPainted(true);
  rc::ListView lv;
  lv.setItems(numberedItems(30));
  lvui::ListBox box;
  box.create(lv_screen_active());
  placeListBox(box);
  box.sync(lv);
  TEST_ASSERT_EQUAL(13, box.shownItems());
  TEST_ASSERT_NULL(box.row(0));
  // The box, its painted rows and the scroll bar's track and thumb.
  TEST_ASSERT_EQUAL(2u + 2u, lv_obj_get_child_count(box.obj()) + lv_obj_get_child_count(lv_obj_get_child(box.obj(), 1)) + 1);
  // An unchanged sync draws nothing again.
  lv_refr_now(nullptr);
  lv_display_t* disp = lv_display_get_default();
  int count = 0;
  auto cb = [](lv_event_t* e) { ++*static_cast<int*>(lv_event_get_user_data(e)); };
  lv_display_add_event_cb(disp, cb, LV_EVENT_INVALIDATE_AREA, &count);
  box.sync(lv);
  TEST_ASSERT_EQUAL(0, count);
  lv.setSelected(1);
  box.sync(lv);
  lv_display_remove_event_cb_with_user_data(disp, cb, &count);
  // The old row and the new one.
  TEST_ASSERT_EQUAL(2, count);
}

void test_list_painted_taps() {
  lvui::ListBox::setPainted(true);
  test_list_tap_emits_activate();
}

void test_overlay_painted_taps() {
  lvui::ListBox::setPainted(true);
  test_overlay_captures_taps();
  keys.clear();
  lv_obj_clean(lv_screen_active());
  lv_obj_clean(lv_layer_top());
  test_overlay_bar_column_is_backdrop();
}

// A tap target acts as the grid's Fixed target: a tap within the slop, a
// long press of a still finger at rc::kLongPressMs; a drag, a moved hold or a
// swipe acts on nothing.
void test_tap_targets_keep_gesture_rules() {
  lvui::TopBar bar;
  bar.create(lv_screen_active());
  lvui::TopBarData d;
  d.title = U"Rooms";
  d.touch.on = true;
  bar.sync(d);
  const lv_point_t back = centre(bar.back());
  struct Case {
    const char* name;
    std::vector<Sample> path;
    int want;
  };
  const Case cases[] = {
      {"still tap", movePath(back, 0, 0, 0, 40), 1},
      {"tap within the slop", movePath(back, 6, 4, 2, 40), 1},
      {"drag past the slop", movePath(back, 0, 40, 8, 40), 0},
      {"sideways drag", movePath(back, 30, 0, 6, 40), 0},
      {"still hold", movePath(back, 0, 0, 0, 40, 600), 1},
      {"hold that moved first", movePath(back, 0, 20, 2, 40, 0, 700), 0},
      {"moved just before the long press", movePath(back, 12, 0, 1, 40, 480), 0},
  };
  for (const Case& c : cases) {
    keys.clear();
    const auto want = gridFixedKeys(bar.back(), Action::Back, Action::OpenHome, c.path);
    TEST_ASSERT_EQUAL_MESSAGE(c.want, want.size(), c.name);
    playSamples(c.path);
    TEST_ASSERT_EQUAL_MESSAGE(want.size(), keys.size(), c.name);
    for (size_t i = 0; i < want.size(); ++i) TEST_ASSERT_TRUE_MESSAGE(want[i].action == keys[i].action, c.name);
  }

  // The long press acts when the still finger reaches rc::kLongPressMs, not
  // before (read on every call, as board_glue sets the pointer).
  keys.clear();
  lv_timer_t* read = lv_indev_get_read_timer(lv_indev_get_next(nullptr));
  lv_timer_set_period(read, 1);
  ptrX = static_cast<int16_t>(back.x);
  ptrY = static_cast<int16_t>(back.y);
  ptrDown = true;
  step();
  const uint32_t down = now;
  while (now - down + 20 < rc::kLongPressMs) step(20);
  TEST_ASSERT_EQUAL(0, keys.size());
  step(20);
  TEST_ASSERT_EQUAL(1, keys.size());
  TEST_ASSERT_TRUE(keys[0].action == Action::OpenHome);
  ptrDown = false;
  step();
  step();
  lv_timer_set_period(read, LV_DEF_REFR_PERIOD);
  TEST_ASSERT_EQUAL(1, keys.size());
}

// An edge swipe that starts on a target, slow enough that LVGL's long press
// would come first, is ui_task's EdgeBack alone: the target emits nothing.
void test_edge_swipe_from_target_emits_nothing() {
  lvui::TopBar bar;
  bar.create(lv_screen_active());
  lvui::TopBarData d;
  d.title = U"Rooms";
  d.touch.on = true;
  bar.sync(d);
  const lv_point_t back = centre(bar.back());
  TEST_ASSERT_TRUE(back.x < 40);
  const auto path = movePath(back, 64, 0, 13, 40);
  bool edge = false;
  const auto want = gridFixedKeys(bar.back(), Action::Back, Action::OpenHome, path, &edge);
  TEST_ASSERT_TRUE(edge);
  TEST_ASSERT_EQUAL(0, want.size());
  TEST_ASSERT_TRUE(path.back().ms - path.front().ms > rc::kLongPressMs);
  playSamples(path);
  TEST_ASSERT_EQUAL(0, keys.size());

  // From the right edge, on the side strip's buttons: a drag down them.
  lvui::SideNavStrip nav;
  nav.create(lv_screen_active());
  nav.sync();
  const auto drag = movePath(centre(nav.button(0)), 0, 60, 12, 50);
  TEST_ASSERT_EQUAL(0, gridFixedKeys(nav.button(0), Action::Up, Action::None, drag).size());
  playSamples(drag);
  TEST_ASSERT_EQUAL(0, keys.size());
}

std::vector<rc::PresenceEntry> people(int n) {
  std::vector<rc::PresenceEntry> out;
  for (int i = 0; i < n; ++i) out.push_back(person(("p" + std::to_string(i)).c_str()));
  return out;
}

// The drawer's names are the grid's wrapping ListId::Drawer: a tap, a drag
// and a flick give the keys its tap map gives, and nothing scrolls natively.
// A long list shows Drawer::render's window, the scroll bar in its last column.
void test_drawer_drag_steps_selection() {
  rc::Drawer d;
  d.setPeople(people(30), 0, 60000);
  lvui::DrawerPanel panel;
  panel.create(lv_screen_active());
  panel.sync(d);
  liveDrawer = &d;
  liveId = ListId::Drawer;
  TEST_ASSERT_FALSE(lv_obj_has_flag(panel.obj(), LV_OBJ_FLAG_SCROLLABLE));
  // 15 rows: the header, then 14 names.
  TEST_ASSERT_EQUAL(14, panel.shownItems());
  TEST_ASSERT_FALSE(lv_obj_has_flag(panel.scrollBar().thumb(), LV_OBJ_FLAG_HIDDEN));
  // The names stop a column short of the bar.
  lv_obj_update_layout(lv_screen_active());
  TEST_ASSERT_EQUAL(13 * 8, lv_obj_get_width(panel.row(0)));

  const int x = 25 * 8 + 4;  // the edge column: a row target too
  const auto tapPath = fingerPath({x, 3 * 16 + 8}, 0, 0, 40);
  const auto wantTap = gridKeys(d, 0, 15, tapPath);
  TEST_ASSERT_EQUAL(1, wantTap.size());
  play(tapPath, panel, d);
  assertSameKeys(wantTap, keys);
  assertActivate(keys[0], 2, ListId::Drawer, Action::Select);
  TEST_ASSERT_EQUAL(2, d.selected());

  keys.clear();
  const auto drag = fingerPath({x + 3 * 8, 8 * 16 + 8}, -3 * 16, 12, 60);
  const auto want = gridKeys(d, 0, 15, drag);
  TEST_ASSERT_EQUAL(3, want.size());
  play(drag, panel, d);
  assertSameKeys(want, keys);
  TEST_ASSERT_EQUAL(5, d.selected());

  keys.clear();
  const auto flick = fingerPath({x + 3 * 8, 10 * 16 + 8}, -80, 0, 40);
  const auto wantFlick = gridKeys(d, 0, 15, flick);
  TEST_ASSERT_EQUAL(1, wantFlick.size());
  playLifting(flick, panel, d);
  assertSameKeys(wantFlick, keys);
  TEST_ASSERT_EQUAL(18, d.selected());
  // The selection past the bottom sits on the last row, as the grid draws it.
  panel.sync(d);
  TEST_ASSERT_EQUAL_STRING("p18", text(panel.name(13)));
  TEST_ASSERT_EQUAL_STRING("p5", text(panel.name(0)));
  TEST_ASSERT_TRUE(lv_obj_has_state(panel.row(13), LV_STATE_CHECKED));
  TEST_ASSERT_EQUAL(0, lv_obj_get_scroll_y(panel.obj()));

  // A drag on the header steps nothing.
  keys.clear();
  const auto head = fingerPath({x + 3 * 8, 8}, 3 * 16, 12, 60);
  TEST_ASSERT_EQUAL(0, gridKeys(d, 0, 15, head).size());
  play(head, panel, d);
  TEST_ASSERT_EQUAL(0, keys.size());
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
  RUN_TEST(test_entry_list_drag_steps_selection);
  RUN_TEST(test_entry_list_swipe_pages);
  RUN_TEST(test_entry_list_draws_its_window_without_scrolling);
  RUN_TEST(test_drawer_rows_and_close);
  RUN_TEST(test_list_box_window_draws_list_view_rows);
  RUN_TEST(test_list_box_window_drag_steps_selection);
  RUN_TEST(test_list_box_drag_names_its_list);
  RUN_TEST(test_overlay_bar_column_is_backdrop);
  RUN_TEST(test_touch_header_rows_match_grid);
  RUN_TEST(test_list_box_painted_matches_labels);
  RUN_TEST(test_menu_overlay_painted_matches_labels);
  RUN_TEST(test_list_box_painted_has_no_row_objects);
  RUN_TEST(test_list_painted_taps);
  RUN_TEST(test_overlay_painted_taps);
  RUN_TEST(test_tap_targets_keep_gesture_rules);
  RUN_TEST(test_edge_swipe_from_target_emits_nothing);
  RUN_TEST(test_drawer_drag_steps_selection);
  return UNITY_END();
}
