// The keyboard, button-row, tile and QR views: what they draw and what a tap
// on them emits, which matches what the grid's tap map gives today.
#include <unity.h>

#include <lvgl.h>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "feature_flags.h"
#include "lvui/cell_font.h"
#include "lvui/display.h"
#include "lvui/input.h"
#include "lvui/lv_init.h"
#include "lvui/styles.h"
#include "lvui/views/button_rows.h"
#include "lvui/views/keyboard.h"
#include "lvui/views/menu_overlay.h"
#include "lvui/views/qr_box.h"
#include "lvui/views/tile_grid.h"
#include "spell/spell.h"
#include "ui/home_items.h"
#include "ui/qr_cells.h"
#include "ui/theme.h"
#include "ui/widgets/osk.h"
#include "ui/widgets/overlay.h"
#include "ui/widgets/spell_strip.h"
#include "ui/widgets/symbol_table.h"
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

void tap(lv_point_t p) {
  ptrX = static_cast<int16_t>(p.x);
  ptrY = static_cast<int16_t>(p.y);
  ptrDown = true;
  step();
  ptrDown = false;
  step();
  step();
}

void layout() {
  lv_obj_update_layout(lv_screen_active());
  lv_obj_update_layout(lv_layer_top());
}

lv_area_t coords(lv_obj_t* o) {
  layout();
  lv_area_t a;
  lv_obj_get_coords(o, &a);
  return a;
}

lv_point_t centre(lv_obj_t* o) {
  const lv_area_t a = coords(o);
  return {(a.x1 + a.x2) / 2, (a.y1 + a.y2) / 2};
}

const char* text(lv_obj_t* label) { return lv_label_get_text(label); }
uint16_t rgb565(lv_color_t c) { return lv_color_to_u16(c); }

// A tiny word list, encoded as tools/gen_spell_dict.py does (test_spell).
const rc::spell::Dict& dict() {
  static const std::pair<const char*, int> kWords[] = {{"the", 255}, {"ten", 150}, {"tea", 100}, {"say", 120}};
  static std::vector<uint8_t> data;
  static std::vector<uint32_t> blocks;
  static rc::spell::Dict d{};
  if (!blocks.empty()) return d;
  std::vector<std::pair<std::string, int>> ws;
  for (const auto& w : kWords) ws.emplace_back(w.first, w.second);
  std::sort(ws.begin(), ws.end());
  std::string prev;
  for (size_t i = 0; i < ws.size(); i++) {
    const std::string& w = ws[i].first;
    size_t p = 0;
    if (i % rc::spell::kBlockWords == 0)
      blocks.push_back(uint32_t(data.size()));
    else
      while (p < std::min({w.size(), prev.size(), size_t(15)}) && w[p] == prev[p]) p++;
    data.push_back(uint8_t((p << 4) | (w.size() - p)));
    for (size_t k = p; k < w.size(); k++) data.push_back(uint8_t(w[k]));
    data.push_back(uint8_t(ws[i].second));
    prev = w;
  }
  d = {data.data(), uint32_t(data.size()), blocks.data(), uint32_t(blocks.size()), uint32_t(ws.size())};
  return d;
}

struct Field {
  std::u32string s;
  const std::u32string& text() const { return s; }
  size_t cursor() const { return s.size(); }
  bool replace(size_t at, size_t n, const std::u32string& w) {
    s.replace(at, n, w);
    return true;
  }
};

}  // namespace

void setUp() {
  lvui::setTickSource(nowMs);
  lvui::init();
  static lv_display_t* disp = lvui::displayBegin({320, 240, noRows});
  (void)disp;
  lvui::pointerBegin(readPtr);
  lvui::setKeySink(sink);
  lvui::setTouchOnly(false);
  lvui::applyTheme(rc::defaultTheme());
  rc::setCursorShown(true);
  rc::Overlay::setConfirmButtons(false);
  lv_obj_clean(lv_screen_active());
  lv_obj_clean(lv_layer_top());
  step();
  keys.clear();
}

void tearDown() {}

// The keyboard is the Osk's page as a button matrix, Osk::rows() rows tall;
// a tap on a key emits what Osk::press makes of it, and the page key turns
// the layer over.
void test_osk_layer_and_emit() {
  const rc::TextSize ts = rc::TextSize::Comfortable;
  rc::Osk osk;
  osk.reset();
  osk.layout(40, ts);  // 320 px of 8 px cells
  lvui::Keyboard kb;
  kb.create(lv_screen_active());
  kb.sync(osk, ts);
  layout();
  TEST_ASSERT_EQUAL(rc::Osk::rows(ts) * 16, lv_obj_get_height(kb.obj()));
  TEST_ASSERT_EQUAL(320, lv_obj_get_width(kb.obj()));

  const int band = rc::Osk::keyRows(ts) * 16;
  auto keyAt = [&](int k) {
    const lv_area_t a = coords(kb.obj());
    const rc::OskKey& key = osk.keys()[k];
    return lv_point_t{int32_t(a.x1 + key.col * 8 + key.w * 8 / 2), int32_t(a.y1 + key.row / 3 * band + band / 2)};
  };
  auto find = [&](rc::OskKind kind, char32_t ch = 0) {
    for (int i = 0; i < int(osk.keys().size()); ++i)
      if (osk.keys()[i].kind == kind && (ch == 0 || osk.keys()[i].ch == ch)) return i;
    return -1;
  };
  TEST_ASSERT_EQUAL_STRING("q", kb.label(0));

  const int q = find(rc::OskKind::Char, U'q');
  tap(keyAt(q));
  TEST_ASSERT_EQUAL(1, keys.size());
  TEST_ASSERT_TRUE(keys[0].action == Action::Char);
  TEST_ASSERT_EQUAL(U'q', keys[0].ch);
  // Lit for a moment after the tap, as the grid draws it.
  kb.sync(osk, ts);
  TEST_ASSERT_TRUE(kb.on(q));
  step(rc::kOskFlashMs);
  kb.sync(osk, ts);
  TEST_ASSERT_FALSE(kb.on(q));

  // Shift stays lit and capitalises the next letter.
  keys.clear();
  const int shift = find(rc::OskKind::Shift);
  tap(keyAt(shift));
  step(rc::kOskFlashMs);
  kb.sync(osk, ts);
  TEST_ASSERT_TRUE(kb.on(shift));
  tap(keyAt(find(rc::OskKind::Char, U'a')));
  TEST_ASSERT_EQUAL(1, keys.size());
  TEST_ASSERT_EQUAL(U'A', keys[0].ch);

  // The page key turns to the symbols: no key for the screen.
  keys.clear();
  tap(keyAt(find(rc::OskKind::Page)));
  TEST_ASSERT_EQUAL(0, keys.size());
  TEST_ASSERT_TRUE(osk.page() == rc::OskPage::Symbols);
  kb.sync(osk, ts);
  TEST_ASSERT_EQUAL_STRING("1", kb.label(0));
  tap(keyAt(find(rc::OskKind::Char, U'1')));
  TEST_ASSERT_EQUAL(1, keys.size());
  TEST_ASSERT_EQUAL(U'1', keys[0].ch);

  // Hide asks the app to put the keyboard away.
  static int hides = 0;
  hides = 0;
  kb.setOnHide([] { ++hides; });
  keys.clear();
  tap(keyAt(find(rc::OskKind::Hide)));
  TEST_ASSERT_EQUAL(1, hides);
  TEST_ASSERT_EQUAL(0, keys.size());
}

// A finger that slides past the tap slop types nothing, even on one wide key
// (as the grid's drag; LVGL alone types it), and the light stays on the key
// it went down on.
void test_osk_slide_types_nothing() {
  const rc::TextSize ts = rc::TextSize::Comfortable;
  rc::Osk osk;
  osk.reset();
  osk.layout(40, ts);
  // On the top layer, as the app docks it: nothing there scrolls, so LVGL
  // itself doesn't cancel a sliding press.
  lv_obj_remove_flag(lv_layer_top(), LV_OBJ_FLAG_SCROLLABLE);
  lvui::Keyboard kb;
  kb.create(lv_layer_top());
  kb.sync(osk, ts);
  layout();
  const int band = rc::Osk::keyRows(ts) * 16;
  int space = -1;
  for (int i = 0; i < int(osk.keys().size()); ++i)
    if (osk.keys()[i].kind == rc::OskKind::Space) space = i;
  TEST_ASSERT_TRUE(space >= 0);
  const rc::OskKey& key = osk.keys()[space];
  TEST_ASSERT_TRUE(key.w * 8 > rc::kTapSlopPx + 8);
  const lv_area_t a = coords(kb.obj());
  const int32_t y = a.y1 + key.row / 3 * band + band / 2;
  const int32_t x0 = a.x1 + key.col * 8 + 2, x1 = x0 + rc::kTapSlopPx + 4;
  keys.clear();
  ptrX = int16_t(x0);
  ptrY = int16_t(y);
  ptrDown = true;
  step();
  kb.sync(osk, ts);
  TEST_ASSERT_TRUE(kb.on(space));
  ptrX = int16_t(x1);
  step();
  step();
  ptrDown = false;
  step();
  step();
  TEST_ASSERT_EQUAL(0, keys.size());
  // A plain tap after it still types.
  tap(lv_point_t{x1, y});
  TEST_ASSERT_EQUAL(1, keys.size());
  TEST_ASSERT_EQUAL(U' ', keys[0].ch);
  lv_obj_delete(kb.obj());
}

// A tap on a symbol emits the tap map's {Activate, i, Menu}, which the table
// turns into typing that symbol; a tap outside the table is Back.
void test_symbols_emit_char() {
  rc::SymbolTable st;
  st.open(U"commands");
  lvui::SymbolBox box;
  box.create(lv_screen_active());
  box.sync(st);
  const rc::Theme& t = rc::defaultTheme();
  TEST_ASSERT_TRUE(lv_obj_has_state(box.cell(0), LV_STATE_CHECKED));
  TEST_ASSERT_EQUAL_HEX16(t.selectBg, rgb565(lv_obj_get_style_bg_color(box.cell(0), LV_PART_MAIN)));
  TEST_ASSERT_EQUAL_HEX16(t.drawerEdge, rgb565(lv_obj_get_style_border_color(box.frame(), LV_PART_MAIN)));
  TEST_ASSERT_EQUAL_STRING("symbols", text(box.title()));

  const int i = 3;
  const char32_t want = st.items()[i];
  tap(centre(box.cell(i)));
  TEST_ASSERT_EQUAL(1, keys.size());
  TEST_ASSERT_TRUE(keys[0].action == Action::Activate);
  TEST_ASSERT_EQUAL(i, keys[0].index);
  TEST_ASSERT_EQUAL(uint8_t(ListId::Menu), keys[0].target);
  const rc::SymbolTable::Out o = st.key(keys[0]);
  TEST_ASSERT_TRUE(o.kind == rc::SymbolTable::Kind::Insert);
  TEST_ASSERT_EQUAL(want, o.ch);

  // The commands row asks for the palette.
  st.open(U"commands");
  box.sync(st);
  keys.clear();
  tap(centre(box.commands()));
  TEST_ASSERT_EQUAL(1, keys.size());
  TEST_ASSERT_TRUE(st.key(keys[0]).kind == rc::SymbolTable::Kind::Commands);

  st.open(U"commands");
  box.sync(st);
  keys.clear();
  tap({2, 2});
  TEST_ASSERT_EQUAL(1, keys.size());
  TEST_ASSERT_TRUE(keys[0].action == Action::Back);
}

// With Unread first and an odd count, Unread's tile spans both columns and
// the rest go two to a row; taps emit Home's row keys.
void test_home_tiles_span_first() {
  std::vector<rc::HomeItem> items = rc::homeItems(rc::Features{true, true, true, true, true});
  if (items.size() % 2 == 0) items.pop_back();
  TEST_ASSERT_TRUE(rc::homeSpanFirst(items));
  const int n = int(items.size());
  const lvui::HomeFit fit = lvui::homeFit(320, 28 * 16, 30, rc::TextSize::Comfortable, n, 90, true);
  TEST_ASSERT_EQUAL(n, int(fit.lay.tiles.size()));
  std::vector<lvui::HomeTileInfo> infos;
  for (int i = 0; i < n; ++i) infos.push_back({U"tile" + rc::utf8ToU32(std::to_string(i)), U"", i == 0});
  lvui::TileGrid grid;
  grid.create(lv_screen_active());
  grid.syncHome(fit, 16, infos, 0, 1);
  TEST_ASSERT_EQUAL(n, grid.count());
  const lv_area_t t0 = coords(grid.tile(0)), t1 = coords(grid.tile(1)), t2 = coords(grid.tile(2));
  TEST_ASSERT_EQUAL(t0.x1, t1.x1);
  TEST_ASSERT_EQUAL(t0.x2, t2.x2);
  TEST_ASSERT_EQUAL(t1.y1, t2.y1);
  TEST_ASSERT_TRUE(t1.x2 < t2.x1);
  TEST_ASSERT_TRUE(t1.y1 > t0.y2);
  const rc::Theme& t = rc::defaultTheme();
  TEST_ASSERT_EQUAL_STRING("tile0", text(grid.label(0, 0)));
  TEST_ASSERT_EQUAL_HEX16(t.selectBg, rgb565(lv_obj_get_style_text_color(grid.label(0, 0), LV_PART_MAIN)));
  TEST_ASSERT_EQUAL_HEX16(t.fg, rgb565(lv_obj_get_style_text_color(grid.label(1, 0), LV_PART_MAIN)));
  TEST_ASSERT_EQUAL_HEX16(t.selectBg, rgb565(lv_obj_get_style_border_color(grid.tile(1), LV_PART_MAIN)));
  TEST_ASSERT_EQUAL_HEX16(t.dim, rgb565(lv_obj_get_style_border_color(grid.tile(0), LV_PART_MAIN)));

  tap(centre(grid.tile(2)));
  TEST_ASSERT_EQUAL(1, keys.size());
  TEST_ASSERT_TRUE(keys[0].action == Action::Activate);
  TEST_ASSERT_EQUAL(2, keys[0].index);
  TEST_ASSERT_EQUAL(uint8_t(ListId::Main), keys[0].target);
  TEST_ASSERT_TRUE(keys[0].then == Action::Select);
}

// A tap on a suggestion emits {Spell, i}; the selection is filled on colour
// themes and outlined in fg on outlined ones.
void test_spell_emit() {
  rc::SpellStrip strip;
  Field field{U"say teh "};
  strip.typed(field, dict(), true);
  TEST_ASSERT_TRUE(strip.visible());
  lvui::SpellBar bar;
  bar.create(lv_screen_active());
  bar.sync(strip);
  TEST_ASSERT_EQUAL(strip.count(), bar.shown());
  TEST_ASSERT_EQUAL_STRING(" teh ", text(bar.option(0)));
  const rc::Theme& t = rc::defaultTheme();
  TEST_ASSERT_EQUAL_HEX16(t.selectBg, rgb565(lv_obj_get_style_bg_color(bar.option(0), LV_PART_MAIN)));
  TEST_ASSERT_EQUAL_HEX16(t.selectFg, rgb565(lv_obj_get_style_text_color(bar.option(0), LV_PART_MAIN)));
  TEST_ASSERT_EQUAL_HEX16(t.fg, rgb565(lv_obj_get_style_text_color(bar.option(1), LV_PART_MAIN)));
  TEST_ASSERT_EQUAL_HEX16(t.inputBg, rgb565(lv_obj_get_style_bg_color(bar.option(1), LV_PART_MAIN)));

  tap(centre(bar.option(1)));
  TEST_ASSERT_EQUAL(1, keys.size());
  TEST_ASSERT_TRUE(keys[0].action == Action::Spell);
  TEST_ASSERT_EQUAL(1, keys[0].index);

  // Outlined: no fill, a 1 px outline in fg round the selected option, and
  // the typed word dim.
  const rc::Theme& paper = rc::paperTheme();
  lvui::applyTheme(paper);
  bar.sync(strip);
  TEST_ASSERT_EQUAL_HEX16(paper.inputBg, rgb565(lv_obj_get_style_bg_color(bar.option(0), LV_PART_MAIN)));
  TEST_ASSERT_EQUAL_HEX16(paper.dim, rgb565(lv_obj_get_style_text_color(bar.option(0), LV_PART_MAIN)));
  TEST_ASSERT_EQUAL(1, lv_obj_get_style_border_width(bar.option(0), LV_PART_MAIN));
  TEST_ASSERT_EQUAL_HEX16(paper.fg, rgb565(lv_obj_get_style_border_color(bar.option(0), LV_PART_MAIN)));
  TEST_ASSERT_EQUAL(0, lv_obj_get_style_border_width(bar.option(1), LV_PART_MAIN));
}

// Touch only: a confirm is the title wrapped in the error colour over two
// buttons, as the grid's overlay draws it; a button emits the menu row's
// key, a tap elsewhere Back.
void test_overlay_confirm_buttons() {
  rc::Overlay::setConfirmButtons(true);
  rc::Overlay ov;
  ov.confirm(U"Leave room?", U"Leave");
  lvui::MenuOverlay menu;
  menu.create();
  menu.setPlace(rc::OverlayPlace::BottomRight, -1, 0, 16);
  menu.sync(ov);
  TEST_ASSERT_TRUE(lv_obj_has_flag(menu.box(), LV_OBJ_FLAG_HIDDEN));
  TEST_ASSERT_FALSE(lv_obj_has_flag(menu.buttons().obj(), LV_OBJ_FLAG_HIDDEN));
  TEST_ASSERT_EQUAL(2, menu.buttons().count());
  const rc::Theme& t = rc::defaultTheme();
  TEST_ASSERT_EQUAL_STRING("Leave room?", text(menu.buttonTitle()));
  TEST_ASSERT_EQUAL_HEX16(t.error, rgb565(lv_obj_get_style_text_color(menu.buttonTitle(), LV_PART_MAIN)));
  // Cancel on the left, the action in the error colour on the right.
  const lv_area_t c = coords(menu.buttons().tile(0)), a = coords(menu.buttons().tile(1));
  TEST_ASSERT_TRUE(c.x2 < a.x1);
  TEST_ASSERT_EQUAL(c.y1, a.y1);
  TEST_ASSERT_EQUAL_STRING("Leave", text(menu.buttons().label(1, 0)));
  TEST_ASSERT_EQUAL_HEX16(t.error, rgb565(lv_obj_get_style_text_color(menu.buttons().label(1, 0), LV_PART_MAIN)));
  TEST_ASSERT_EQUAL_HEX16(t.dim, rgb565(lv_obj_get_style_border_color(menu.buttons().tile(1), LV_PART_MAIN)));

  tap(centre(menu.buttons().tile(1)));
  TEST_ASSERT_EQUAL(1, keys.size());
  TEST_ASSERT_TRUE(keys[0].action == Action::Activate);
  TEST_ASSERT_EQUAL(1, keys[0].index);
  TEST_ASSERT_EQUAL(uint8_t(ListId::Menu), keys[0].target);
  keys.clear();
  tap({4, 20});
  TEST_ASSERT_EQUAL(1, keys.size());
  TEST_ASSERT_TRUE(keys[0].action == Action::Back);

  // With keys: the usual list box.
  rc::Overlay::setConfirmButtons(false);
  menu.sync(ov);
  TEST_ASSERT_FALSE(lv_obj_has_flag(menu.box(), LV_OBJ_FLAG_HIDDEN));
  TEST_ASSERT_TRUE(lv_obj_has_flag(menu.buttons().obj(), LV_OBJ_FLAG_HIDDEN));
}

// The same box shown as a list, then as the same confirm again (feed_list.cpp,
// profile.cpp): the title and the panel come back with the buttons.
void test_overlay_confirm_after_list() {
  rc::Overlay::setConfirmButtons(true);
  rc::Overlay ov;
  ov.confirm(U"Leave room?", U"Leave");
  lvui::MenuOverlay menu;
  menu.create();
  menu.setPlace(rc::OverlayPlace::BottomRight, -1, 0, 16);
  menu.sync(ov);
  ov.open(U"Commands", {rc::ListItem{U"Reply"}, rc::ListItem{U"Leave"}});
  menu.sync(ov);
  TEST_ASSERT_TRUE(lv_obj_has_flag(menu.buttonTitle(), LV_OBJ_FLAG_HIDDEN));
  ov.confirm(U"Leave room?", U"Leave");
  menu.sync(ov);
  TEST_ASSERT_FALSE(lv_obj_has_flag(menu.buttons().obj(), LV_OBJ_FLAG_HIDDEN));
  TEST_ASSERT_FALSE(lv_obj_has_flag(menu.buttonTitle(), LV_OBJ_FLAG_HIDDEN));
  TEST_ASSERT_EQUAL_STRING("Leave room?", text(menu.buttonTitle()));
  // The panel is the backdrop's first child, behind the title.
  TEST_ASSERT_FALSE(lv_obj_has_flag(lv_obj_get_child(menu.backdrop(), 1), LV_OBJ_FLAG_HIDDEN));
  rc::Overlay::setConfirmButtons(false);
}

// An unchanged confirm is laid out once: a later sync leaves the tiles alone.
void test_overlay_buttons_sync_once() {
  rc::Overlay::setConfirmButtons(true);
  rc::Overlay ov;
  ov.confirm(U"Leave room?", U"Leave");
  lvui::MenuOverlay menu;
  menu.create();
  menu.sync(ov);
  lv_obj_t* l = menu.buttons().label(1, 0);
  lv_label_set_text(l, "marker");
  menu.sync(ov);
  menu.sync(ov);
  TEST_ASSERT_EQUAL_STRING("marker", text(l));
  // A change lays it out again.
  ov.confirm(U"Leave room?", U"Go");
  menu.sync(ov);
  TEST_ASSERT_EQUAL_STRING("Go", text(menu.buttons().label(1, 0)));
  rc::Overlay::setConfirmButtons(false);
}

// A touch-only button menu (renderButtonMenu): the title in fg on the page
// above the buttons, no panel, two a row; a tap activates the item.
void test_overlay_button_menu() {
  rc::Overlay::setConfirmButtons(true);
  rc::Overlay ov;
  ov.openButtons(U"Entry", {rc::ListItem{U"Reply"}, rc::ListItem{U"Like"}, rc::ListItem{U"Delete", U"", rc::ItemTone::Error}});
  lvui::MenuOverlay menu;
  menu.create();
  menu.setPlace(rc::OverlayPlace::BottomRight, -1, 0, 16);
  menu.sync(ov);
  TEST_ASSERT_TRUE(lv_obj_has_flag(menu.box(), LV_OBJ_FLAG_HIDDEN));
  TEST_ASSERT_FALSE(lv_obj_has_flag(menu.buttons().obj(), LV_OBJ_FLAG_HIDDEN));
  TEST_ASSERT_TRUE(lv_obj_has_flag(lv_obj_get_child(menu.backdrop(), 1), LV_OBJ_FLAG_HIDDEN));
  TEST_ASSERT_EQUAL(3, menu.buttons().count());
  const rc::Theme& t = rc::defaultTheme();
  TEST_ASSERT_EQUAL_STRING("Entry", text(menu.buttonTitle()));
  TEST_ASSERT_EQUAL_HEX16(t.fg, rgb565(lv_obj_get_style_text_color(menu.buttonTitle(), LV_PART_MAIN)));
  const lv_area_t a = coords(menu.buttons().tile(0)), b = coords(menu.buttons().tile(1)),
                  c = coords(menu.buttons().tile(2));
  TEST_ASSERT_TRUE(a.x2 < b.x1);
  TEST_ASSERT_EQUAL(a.y1, b.y1);
  TEST_ASSERT_EQUAL(a.x1, c.x1);
  TEST_ASSERT_TRUE(c.y1 > a.y2);
  TEST_ASSERT_TRUE(coords(menu.buttonTitle()).y2 < a.y1);
  TEST_ASSERT_EQUAL_STRING("Delete", text(menu.buttons().label(2, 0)));
  TEST_ASSERT_EQUAL_HEX16(t.error, rgb565(lv_obj_get_style_text_color(menu.buttons().label(2, 0), LV_PART_MAIN)));
  keys.clear();
  tap(centre(menu.buttons().tile(2)));
  TEST_ASSERT_EQUAL(1, keys.size());
  TEST_ASSERT_TRUE(keys[0].action == Action::Activate);
  TEST_ASSERT_EQUAL(2, keys[0].index);
  TEST_ASSERT_EQUAL(uint8_t(ListId::Menu), keys[0].target);
  keys.clear();
  rc::Overlay::setConfirmButtons(false);
  ov.close();
  menu.sync(ov);
}

// The QR modules as qr_cells makes them: black on white, two cells of margin
// either side and a row above and below.
void test_qr_box_rows() {
  std::vector<std::u32string> want;
  TEST_ASSERT_TRUE(rc::qrToCells("https://cyberspace.online", want));
  lvui::QrBox qr;
  qr.create(lv_screen_active());
  TEST_ASSERT_TRUE(qr.sync("https://cyberspace.online"));
  TEST_ASSERT_EQUAL(int(want.size()) + 2, qr.rows());
  TEST_ASSERT_EQUAL(int(want[0].size()) + 4, qr.cols());
  TEST_ASSERT_EQUAL_STRING(rc::u32ToUtf8(want[3]).c_str(), text(qr.row(3)));
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, rgb565(lv_obj_get_style_bg_color(qr.obj(), LV_PART_MAIN)));
  TEST_ASSERT_EQUAL_HEX16(0x0000, rgb565(lv_obj_get_style_text_color(qr.row(3), LV_PART_MAIN)));
  layout();
  TEST_ASSERT_EQUAL(qr.cols() * 8, lv_obj_get_width(qr.obj()));
  TEST_ASSERT_EQUAL(qr.rows() * 16, lv_obj_get_height(qr.obj()));
}

// A text-size change re-lays the rows in the new cell, with the URL the same.
void test_qr_box_follows_cell_size() {
  lv_obj_t* parent = lvui::plainBox(lv_screen_active());
  lv_obj_set_size(parent, lv_pct(100), lv_pct(100));
  lvui::QrBox qr;
  qr.create(parent);
  TEST_ASSERT_TRUE(qr.sync("https://cyberspace.online"));
  layout();
  TEST_ASSERT_EQUAL(4 * 16, lv_obj_get_y(qr.row(3)));
  lv_obj_set_style_text_font(parent, lvui::font(rc::TextSize::Compact), LV_PART_MAIN);
  TEST_ASSERT_TRUE(qr.sync("https://cyberspace.online"));
  layout();
  const lvui::Cell c = lvui::cellOf(qr.obj());
  TEST_ASSERT_EQUAL(12, c.h);
  TEST_ASSERT_EQUAL(qr.cols() * c.w, lv_obj_get_width(qr.obj()));
  TEST_ASSERT_EQUAL(qr.rows() * c.h, lv_obj_get_height(qr.obj()));
  TEST_ASSERT_EQUAL(4 * c.h, lv_obj_get_y(qr.row(3)));
  TEST_ASSERT_EQUAL(2 * c.w, lv_obj_get_x(qr.row(3)));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_osk_layer_and_emit);
  RUN_TEST(test_osk_slide_types_nothing);
  RUN_TEST(test_symbols_emit_char);
  RUN_TEST(test_home_tiles_span_first);
  RUN_TEST(test_spell_emit);
  RUN_TEST(test_overlay_confirm_buttons);
  RUN_TEST(test_overlay_confirm_after_list);
  RUN_TEST(test_overlay_buttons_sync_once);
  RUN_TEST(test_overlay_button_menu);
  RUN_TEST(test_qr_box_rows);
  RUN_TEST(test_qr_box_follows_cell_size);
  return UNITY_END();
}
