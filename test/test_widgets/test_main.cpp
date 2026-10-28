#include <cstdlib>
#include <unity.h>

#include <string>
#include <vector>

#include "input/key_event.h"
#include "ui/grid.h"
#include "ui/qr_cells.h"
#include "ui/theme.h"
#include "ui/widgets/banner.h"
#include "ui/widgets/overlay.h"
#include "ui/widgets/drawer.h"
#include "ui/widgets/entry_list.h"
#include "ui/widgets/input_line.h"
#include "ui/widgets/list_view.h"
#include "ui/widgets/log_view.h"
#include "ui/widgets/palette.h"
#include "ui/widgets/status_bar.h"
#include "ui/widgets/symbol_table.h"
#include "ui/widgets/text_area.h"
#include "input/type_filter.h"
#include "ui/tap_map.h"
#include "ui/emoji_set.h"
#include "ui/text_width.h"
#include "ui/home_tiles.h"
#include "util/utf8.h"

using namespace rc;

void setUp() {}
void tearDown() { setActiveTaps(nullptr); }

static const Theme& T() { return defaultTheme(); }

static std::string rowText(const Grid& g, int row, int from = 0, int to = -1) {
  if (to < 0) to = g.cols();
  std::u32string s;
  for (int c = from; c < to; ++c) s += g.at(c, row).cp;
  return u32ToUtf8(s);
}

static DisplayLine line(const std::string& id, const std::string& text, bool divider = false) {
  DisplayLine l;
  l.messageId = divider ? "" : id;
  l.divider = divider;
  l.spans.push_back({utf8ToU32(text), 1});
  return l;
}

void test_input_insert_cursor() {
  InputLine in;
  for (char32_t c : std::u32string(U"hllo")) in.insert(c);
  TEST_ASSERT_EQUAL(4, in.cursor());
  for (int i = 0; i < 3; ++i) TEST_ASSERT_TRUE(in.left());
  in.insert(U'e');
  TEST_ASSERT_EQUAL_STRING("hello", in.utf8().c_str());
  TEST_ASSERT_EQUAL(2, in.cursor());
  in.setText(U"abc");
  TEST_ASSERT_EQUAL(3, in.cursor());
  in.clear();
  TEST_ASSERT_EQUAL_STRING("", in.utf8().c_str());
  TEST_ASSERT_FALSE(in.left());
}

void test_input_right_at_end_false() {
  InputLine in;
  in.setText(U"ab");
  TEST_ASSERT_TRUE(in.atEnd());
  TEST_ASSERT_FALSE(in.right());
  TEST_ASSERT_TRUE(in.left());
  TEST_ASSERT_FALSE(in.atEnd());
  TEST_ASSERT_TRUE(in.right());
}

void test_input_byte_cap() {
  InputLine in;
  for (int i = 0; i < 2049; ++i) in.insert(U'a');
  TEST_ASSERT_EQUAL(2048, in.utf8().size());
  in.backspace();
  in.insert(U'\U0001F3B2');  // 4 bytes don't fit in the 1 byte left
  TEST_ASSERT_EQUAL(2047, in.utf8().size());
}

void test_input_backspace_multibyte() {
  InputLine in;
  in.insert(U'a');
  in.insert(U'\U0001F3B2');
  in.backspace();
  TEST_ASSERT_EQUAL_STRING("a", in.utf8().c_str());
  in.backspace();
  in.backspace();
  TEST_ASSERT_EQUAL_STRING("", in.utf8().c_str());
}

void test_input_hscroll() {
  Grid g(10, 2);
  InputLine in;
  in.setText(U"abcdefghijklmnopqrst");
  in.render(g, 1, T());
  TEST_ASSERT_EQUAL_STRING("> ", rowText(g, 1, 0, 2).c_str());
  TEST_ASSERT_EQUAL_HEX32(U't', g.at(8, 1).cp);
  TEST_ASSERT_EQUAL_HEX16(T().cursorBg, g.at(9, 1).bg);
  in.setDisabled(true);
  in.render(g, 1, T());
  TEST_ASSERT_EQUAL_HEX16(T().dim, g.at(8, 1).fg);
  TEST_ASSERT_NOT_EQUAL(T().cursorBg, g.at(9, 1).bg);
}

void test_log_follow_and_select() {
  LogView v;
  v.setLines({line("", "-- day --", true), line("a", "one"), line("b", "two"),
              line("b", "  two more"), line("", "-- day --", true), line("c", "three")});
  TEST_ASSERT_TRUE(v.following());
  v.up();
  TEST_ASSERT_FALSE(v.following());
  TEST_ASSERT_EQUAL_STRING("c", v.selectedId().c_str());
  v.up();
  TEST_ASSERT_EQUAL_STRING("b", v.selectedId().c_str());
  v.up();
  TEST_ASSERT_EQUAL_STRING("a", v.selectedId().c_str());
  TEST_ASSERT_TRUE(v.atTop());
  v.up();
  TEST_ASSERT_EQUAL_STRING("a", v.selectedId().c_str());
  v.down();
  v.down();
  TEST_ASSERT_EQUAL_STRING("c", v.selectedId().c_str());
  v.down();
  TEST_ASSERT_TRUE(v.following());
  TEST_ASSERT_EQUAL_STRING("", v.selectedId().c_str());
}

void test_log_render_follow_and_select() {
  std::vector<DisplayLine> ls;
  for (int i = 0; i < 10; ++i) ls.push_back(line("m" + std::to_string(i), "line" + std::to_string(i)));
  ls[9].mentionTint = true;
  LogView v;
  v.setLines(ls);
  Grid g(12, 5);
  v.render(g, 1, 4, 2, T());  // rows 1..3, 10 cols wide
  TEST_ASSERT_EQUAL_STRING("line7", rowText(g, 1, 0, 5).c_str());
  TEST_ASSERT_EQUAL_STRING("line9", rowText(g, 3, 0, 5).c_str());
  TEST_ASSERT_EQUAL_HEX16(T().mentionBg, g.at(0, 3).bg);
  TEST_ASSERT_EQUAL_HEX16(T().mentionBg, g.at(9, 3).bg);
  TEST_ASSERT_NOT_EQUAL(T().mentionBg, g.at(10, 3).bg);  // right reserve untouched
  for (int i = 0; i < 5; ++i) v.up();  // selects m5
  v.render(g, 1, 4, 2, T());
  TEST_ASSERT_EQUAL_STRING("line5", rowText(g, 3, 0, 5).c_str());
  // Selection is an outline, not a fill: colours stay, the box sits on the edges.
  TEST_ASSERT_EQUAL_HEX16(T().bg, g.at(0, 3).bg);
  TEST_ASSERT_EQUAL_HEX16(1, g.at(0, 3).fg);
  TEST_ASSERT_EQUAL(kEdgeTop | kEdgeBottom | kEdgeLeft, g.at(0, 3).edges);
  TEST_ASSERT_EQUAL(kEdgeTop | kEdgeBottom | kEdgeRight, g.at(9, 3).edges);
  TEST_ASSERT_EQUAL_HEX16(T().selectBg, g.at(0, 3).edgeColor);
  TEST_ASSERT_EQUAL(0, g.at(10, 3).edges);  // right reserve untouched
  TEST_ASSERT_EQUAL(0, g.at(0, 2).edges);
}

void test_log_outlines_whole_message() {
  LogView v;
  v.setLines({line("a", "one"), line("b", "two"), line("b", "two more"), line("c", "three")});
  v.up();
  v.up();  // selects b, two lines
  TEST_ASSERT_EQUAL_STRING("b", v.selectedId().c_str());
  Grid g(10, 6);
  v.render(g, 0, 4, 0, T());
  TEST_ASSERT_EQUAL(kEdgeTop | kEdgeLeft, g.at(0, 1).edges);
  TEST_ASSERT_EQUAL(kEdgeBottom | kEdgeLeft, g.at(0, 2).edges);
  TEST_ASSERT_EQUAL(kEdgeBottom | kEdgeRight, g.at(9, 2).edges);
  TEST_ASSERT_EQUAL(0, g.at(0, 3).edges);
}

void test_grid_outline() {
  Grid g(6, 4);
  g.clearDirty();
  g.outline(1, 1, 4, 2, 7);
  TEST_ASSERT_TRUE(g.rowDirty(1));
  TEST_ASSERT_FALSE(g.rowDirty(0));
  TEST_ASSERT_EQUAL(kEdgeTop | kEdgeLeft, g.at(1, 1).edges);
  TEST_ASSERT_EQUAL(kEdgeTop, g.at(2, 1).edges);
  TEST_ASSERT_EQUAL(kEdgeBottom | kEdgeRight, g.at(4, 2).edges);
  TEST_ASSERT_EQUAL(0, g.at(0, 1).edges);
  TEST_ASSERT_EQUAL_HEX16(7, g.at(1, 1).edgeColor);
  g.put(1, 1, U'x', 1, 0);  // a fresh put clears the edges
  TEST_ASSERT_EQUAL(0, g.at(1, 1).edges);
}

void test_log_preserves_selection_on_setLines() {
  LogView v;
  v.setLines({line("a", "one"), line("b", "two"), line("c", "three")});
  v.up();
  v.up();
  TEST_ASSERT_EQUAL_STRING("b", v.selectedId().c_str());
  v.setLines({line("z", "older"), line("a", "one"), line("b", "two"), line("c", "three"),
              line("d", "four")});
  TEST_ASSERT_EQUAL_STRING("b", v.selectedId().c_str());
  v.down();
  TEST_ASSERT_EQUAL_STRING("c", v.selectedId().c_str());
  v.setLines({line("a", "one")});  // selection gone
  TEST_ASSERT_TRUE(v.following());
}

void test_list_scrolls() {
  std::vector<ListItem> items;
  for (int i = 0; i < 10; ++i) items.push_back({utf8ToU32("item" + std::to_string(i)), U""});
  items[7].marker = true;
  items[7].right = U"3";
  ListView lv;
  lv.setItems(items);
  for (int i = 0; i < 7; ++i) lv.down();
  TEST_ASSERT_EQUAL(7, lv.selected());
  Grid g(20, 6);
  lv.render(g, 1, 5, 0, 20, T());  // 4 rows
  TEST_ASSERT_EQUAL_STRING("\xe2\x80\xa2 item7", rowText(g, 4, 0, 7).c_str());
  TEST_ASSERT_EQUAL_HEX32(U'3', g.at(18, 4).cp);
  TEST_ASSERT_EQUAL_HEX16(T().bg, g.at(0, 4).bg);
  TEST_ASSERT_EQUAL(kEdgeTop | kEdgeBottom | kEdgeLeft, g.at(0, 4).edges);
  TEST_ASSERT_EQUAL(kEdgeTop | kEdgeBottom | kEdgeRight, g.at(19, 4).edges);
  TEST_ASSERT_EQUAL(0, g.at(0, 3).edges);
  TEST_ASSERT_EQUAL_STRING("  item4", rowText(g, 1, 0, 7).c_str());
}

void test_cursor_hidden() {
  // Boards with no keys draw no selection outline; taps pick by position.
  std::vector<ListItem> items = {{U"a", U""}, {U"b", U""}};
  ListView lv;
  lv.setItems(items);
  LogView log;
  log.setLines({line("a", "one"), line("b", "two")});
  log.up();
  setCursorShown(false);
  Grid g(20, 6);
  lv.render(g, 0, 2, 0, 20, T());
  log.render(g, 3, 6, 0, T());
  setCursorShown(true);
  for (int r = 0; r < 6; ++r)
    for (int c = 0; c < 20; ++c) TEST_ASSERT_EQUAL(0, g.at(c, r).edges);
}

void test_list_wraps() {
  ListView lv;
  lv.up();  // empty: no-op
  TEST_ASSERT_EQUAL(0, lv.selected());
  std::vector<ListItem> items;
  for (int i = 0; i < 10; ++i) items.push_back({utf8ToU32("item" + std::to_string(i)), U""});
  lv.setItems(items);
  lv.up();
  TEST_ASSERT_EQUAL(9, lv.selected());
  Grid g(20, 6);
  lv.render(g, 1, 5, 0, 20, T());
  TEST_ASSERT_EQUAL_STRING("  item9", rowText(g, 4, 0, 7).c_str());
  lv.down();
  TEST_ASSERT_EQUAL(0, lv.selected());
  lv.setSelected(42);  // direct selection still clamps
  TEST_ASSERT_EQUAL(9, lv.selected());
}

void test_list_colours_follow_theme() {
  ListView lv;
  lv.setItems({ListItem{U"Go", U""}, ListItem{U"Erase", U"", ItemTone::Error}});
  Grid g(20, 3);
  for (ThemeId id : {ThemeId::Default, ThemeId::BlackWhite, ThemeId::WhiteBlack}) {
    const Theme& t = theme(id);
    lv.render(g, 0, 2, 0, 20, t);
    TEST_ASSERT_EQUAL_HEX16(t.fg, g.at(2, 0).fg);
    TEST_ASSERT_EQUAL_HEX16(t.bg, g.at(2, 0).bg);
    TEST_ASSERT_EQUAL_HEX16(t.error, g.at(2, 1).fg);
  }
}

void test_drawer_wraps() {
  PresenceEntry a, b;
  a.username = "rain";
  b.username = "tardibyte";
  Drawer d;
  d.up();  // empty: no-op
  TEST_ASSERT_EQUAL_STRING("", d.selectedUsername().c_str());
  d.setPeople({a, b}, 0, 300000);
  d.up();
  TEST_ASSERT_EQUAL_STRING("tardibyte", d.selectedUsername().c_str());
  d.down();
  TEST_ASSERT_EQUAL_STRING("rain", d.selectedUsername().c_str());
}

void test_drawer_idle_marker() {
  PresenceEntry a, b;
  a.username = "rain";
  a.online = true;
  a.lastActivity = 1000;
  b.username = "tardibyte";
  b.online = true;
  b.lastActivity = -1;
  Drawer d;
  d.setPeople({a, b}, 1000 + 600000, 300000);
  TEST_ASSERT_EQUAL(15, Drawer::width(53));
  TEST_ASSERT_EQUAL(15, Drawer::width(30));
  TEST_ASSERT_EQUAL(12, Drawer::width(24));  // capped at half the grid
  // Docked on a wide panel: a fifth of the grid, at least 15.
  TEST_ASSERT_TRUE(Drawer::docks(480, 320));   // CrowPanel 3.5" landscape
  TEST_ASSERT_FALSE(Drawer::docks(320, 480));  // and portrait
  TEST_ASSERT_TRUE(Drawer::docks(800, 480));
  TEST_ASSERT_FALSE(Drawer::docks(480, 800));
  TEST_ASSERT_FALSE(Drawer::docks(320, 240));  // T-Deck
  TEST_ASSERT_EQUAL(15, Drawer::width(48, true));
  TEST_ASSERT_EQUAL(26, Drawer::width(133, true));
  TEST_ASSERT_EQUAL(16, Drawer::width(80, true));
  TEST_ASSERT_EQUAL(15, Drawer::width(53, true));
  Grid g(40, 6);
  d.render(g, 1, 6, T());
  int x = 40 - 15;
  TEST_ASSERT_EQUAL_STRING("here \xc2\xb7 2", rowText(g, 1, x + 1, x + 9).c_str());
  TEST_ASSERT_EQUAL_STRING("rain z", rowText(g, 2, x + 1, x + 7).c_str());
  TEST_ASSERT_EQUAL_STRING("tardibyte ", rowText(g, 3, x + 1, x + 11).c_str());
  TEST_ASSERT_EQUAL_HEX16(T().drawerBg, g.at(39, 5).bg);
  TEST_ASSERT_EQUAL_HEX16(T().drawerBg, g.at(x + 1, 2).bg);
  TEST_ASSERT_EQUAL(kEdgeTop | kEdgeBottom | kEdgeLeft, g.at(x + 1, 2).edges);
  TEST_ASSERT_EQUAL(kEdgeTop | kEdgeBottom | kEdgeRight, g.at(39, 2).edges);
  TEST_ASSERT_EQUAL(0, g.at(x + 1, 3).edges);
  TEST_ASSERT_EQUAL_STRING("rain", d.selectedUsername().c_str());
  d.down();
  TEST_ASSERT_EQUAL_STRING("tardibyte", d.selectedUsername().c_str());
}

void test_status_bar_layout() {
  Grid g(40, 2);
  renderStatusBar(g, T(), U"#general", 12, U'\u25cf', 3, 87, "14:02");
  TEST_ASSERT_EQUAL_STRING(" #general", rowText(g, 0, 0, 9).c_str());
  TEST_ASSERT_EQUAL_STRING("14:02 ", rowText(g, 0, 34, 40).c_str());
  TEST_ASSERT_TRUE(rowText(g, 0).find("12") != std::string::npos);
  TEST_ASSERT_TRUE(rowText(g, 0).find("\xe2\x97\x8f") != std::string::npos);
  TEST_ASSERT_TRUE(rowText(g, 0).find("87%") != std::string::npos);
  TEST_ASSERT_EQUAL_HEX16(T().barBg, g.at(20, 0).bg);
}

void test_banner_expires() {
  Banner b;
  TEST_ASSERT_FALSE(b.visible(0));
  b.show(U"Sent too fast", 1000, 3000, true);
  TEST_ASSERT_TRUE(b.visible(3999));
  TEST_ASSERT_FALSE(b.visible(4000));
  Grid g(20, 3);
  b.render(g, 2, T(), 2000);
  TEST_ASSERT_EQUAL_STRING(" Sent too fast", rowText(g, 2, 0, 14).c_str());
  TEST_ASSERT_EQUAL_HEX16(T().error, g.at(19, 2).bg);
}

void test_palette_lists_commands() {
  const auto& p = paletteEntries();
  TEST_ASSERT_EQUAL(25, p.size());
  TEST_ASSERT_EQUAL_STRING("/me ", u32ToUtf8(p[0].insert).c_str());
  TEST_ASSERT_EQUAL_STRING("/muted ", u32ToUtf8(p.back().insert).c_str());
}

void test_palette_cmail_drops_circ_only() {
  const auto& p = paletteEntries(PaletteKind::Cmail);
  bool art = false, muted = false, me = false;
  for (const auto& e : p) {
    std::string s = u32ToUtf8(e.insert);
    art |= s == "/art ";
    muted |= s == "/muted ";
    me |= s == "/me ";
  }
  TEST_ASSERT_FALSE(art);
  TEST_ASSERT_FALSE(muted);
  TEST_ASSERT_TRUE(me);
  TEST_ASSERT_EQUAL(int(paletteEntries().size()) - 2, int(p.size()));
}

void test_qr_small_url() {
  std::vector<std::u32string> rows;
  TEST_ASSERT_TRUE(qrToCells("https://tenor.com/x", rows));
  TEST_ASSERT_TRUE(rows.size() <= 15);
  // Version 2: 25 modules + 2 quiet = 27 wide, 14 rows.
  TEST_ASSERT_EQUAL(14, rows.size());
  TEST_ASSERT_EQUAL(27, rows[0].size());
  for (const auto& r : rows)
    for (char32_t c : r)
      TEST_ASSERT_TRUE(c == U' ' || c == U'\u2588' || c == U'\u2580' || c == U'\u2584');
  // Finder pattern: top-left corner after the quiet zone is dark.
  TEST_ASSERT_EQUAL_HEX32(U'\u2584', rows[0][1]);
  TEST_ASSERT_FALSE(qrToCells(std::string(272, 'x'), rows));
}

static std::vector<FeedRow> feedRows(int n) {
  std::vector<FeedRow> rows;
  for (int i = 0; i < n; ++i)
    rows.push_back({utf8ToU32("nick" + std::to_string(i)), utf8ToU32("title" + std::to_string(i)),
                    1, 2});
  return rows;
}

void test_entry_list_outline() {
  EntryList el;
  el.setRows(feedRows(3));
  el.down();
  TEST_ASSERT_EQUAL(1, el.selected());
  Grid g(20, 8);
  el.render(g, 0, 8, T());
  TEST_ASSERT_EQUAL_STRING("nick1", rowText(g, 2, 0, 5).c_str());
  TEST_ASSERT_EQUAL_STRING("title1", rowText(g, 3, 0, 6).c_str());
  TEST_ASSERT_EQUAL(kEdgeTop | kEdgeLeft, g.at(0, 2).edges);
  TEST_ASSERT_EQUAL(kEdgeBottom | kEdgeLeft, g.at(0, 3).edges);
  TEST_ASSERT_EQUAL(kEdgeTop, g.at(5, 2).edges);
  TEST_ASSERT_EQUAL(kEdgeBottom | kEdgeRight, g.at(19, 3).edges);
  TEST_ASSERT_EQUAL(0, g.at(0, 1).edges);
  TEST_ASSERT_EQUAL(0, g.at(0, 4).edges);
  TEST_ASSERT_EQUAL_HEX16(T().bg, g.at(0, 2).bg);
  TEST_ASSERT_EQUAL_HEX16(2, g.at(0, 3).fg);
}

void test_entry_list_scrolls() {
  EntryList el;
  el.setRows(feedRows(6));
  el.setSelected(5);
  Grid g(20, 5);
  el.render(g, 1, 5, T());  // 4 rows = 2 items
  TEST_ASSERT_EQUAL_STRING("nick5", rowText(g, 3, 0, 5).c_str());
  TEST_ASSERT_EQUAL_STRING("nick4", rowText(g, 1, 0, 5).c_str());
  el.setSelected(99);
  TEST_ASSERT_EQUAL(5, el.selected());
}

void test_entry_list_big_heads() {
  EntryList el;
  el.setRows(feedRows(12));
  el.setBigHeads(12, 14, 8);  // 12 px rows, 14 px heads cut to 8 columns
  el.setSelected(1);
  Grid g(10, 20);
  el.render(g, 1, 19, T());  // 18 rows = 216 px; 26 px an entry = 8 entries
  TEST_ASSERT_TRUE(g.rowKind(1) == RowKind::Big);
  TEST_ASSERT_TRUE(g.rowKind(2) == RowKind::Normal);
  TEST_ASSERT_TRUE(g.rowKind(15) == RowKind::Big);
  TEST_ASSERT_TRUE(g.rowKind(16) == RowKind::Normal);
  TEST_ASSERT_TRUE(g.rowKind(17) == RowKind::Skip);
  TEST_ASSERT_TRUE(g.rowKind(18) == RowKind::Skip);
  TEST_ASSERT_TRUE(g.rowKind(19) == RowKind::Normal);
  TEST_ASSERT_EQUAL_STRING("nick1", rowText(g, 3, 0, 5).c_str());
  TEST_ASSERT_EQUAL_STRING("title1", rowText(g, 4, 0, 6).c_str());
  // The head's outline ends at its last column; the text row's at the grid's.
  TEST_ASSERT_EQUAL(kEdgeTop | kEdgeLeft, g.at(0, 3).edges);
  TEST_ASSERT_EQUAL(kEdgeTop | kEdgeRight, g.at(7, 3).edges);
  TEST_ASSERT_EQUAL(0, g.at(8, 3).edges);
  TEST_ASSERT_EQUAL(kEdgeBottom | kEdgeLeft, g.at(0, 4).edges);
  TEST_ASSERT_EQUAL(kEdgeBottom | kEdgeRight, g.at(9, 4).edges);
  // Heads are cut to their own width.
  el.setRows({{U"abcdefghijk", U"x", 1, 2}});
  g.fill(0);
  el.render(g, 1, 19, T());
  TEST_ASSERT_EQUAL_STRING("abcdefgh", rowText(g, 1, 0, 8).c_str());
  TEST_ASSERT_TRUE(g.at(8, 1).cp == U' ');
  // Scrolling keeps the selection in the 8 visible entries.
  el.setRows(feedRows(12));
  el.setSelected(11);
  el.render(g, 1, 19, T());
  TEST_ASSERT_EQUAL_STRING("nick4", rowText(g, 1, 0, 5).c_str());
}

void test_entry_list_wraps() {
  EntryList el;
  el.up();
  el.down();
  TEST_ASSERT_EQUAL(0, el.selected());
  TEST_ASSERT_EQUAL(0, el.size());
  el.setRows(feedRows(3));
  TEST_ASSERT_FALSE(el.atEnd());
  el.up();
  TEST_ASSERT_EQUAL(2, el.selected());
  TEST_ASSERT_TRUE(el.atEnd());
  el.down();
  TEST_ASSERT_EQUAL(0, el.selected());
}

static void typeInto(TextArea& ta, const std::u32string& s) {
  for (char32_t c : s) ta.insert(c);
}

void test_text_area_newlines_and_cap() {
  TextArea ta;
  typeInto(ta, U"ab\ncd");
  TEST_ASSERT_EQUAL_STRING("ab\ncd", ta.utf8().c_str());
  TEST_ASSERT_EQUAL(5, ta.length());
  ta.backspace();
  TEST_ASSERT_EQUAL_STRING("ab\nc", ta.utf8().c_str());
  TEST_ASSERT_TRUE(ta.left());
  ta.backspace();  // removes the '\n'
  TEST_ASSERT_EQUAL_STRING("abc", ta.utf8().c_str());
  ta.clear();
  TEST_ASSERT_EQUAL(0, ta.length());
  TEST_ASSERT_FALSE(ta.left());
  TEST_ASSERT_FALSE(ta.right());

  TextArea small(3);
  TEST_ASSERT_TRUE(small.insert(U'\u00e9'));
  TEST_ASSERT_TRUE(small.insert(U'\U0001F3B2'));
  TEST_ASSERT_TRUE(small.insert(U'x'));
  TEST_ASSERT_FALSE(small.insert(U'y'));
  TEST_ASSERT_EQUAL(3, small.length());
}

void test_text_area_up_down() {
  TextArea ta;
  typeInto(ta, U"first\nsecond");
  TEST_ASSERT_FALSE(ta.down(40));
  TEST_ASSERT_TRUE(ta.up(40));
  TEST_ASSERT_FALSE(ta.up(40));
  ta.insert(U'X');  // cursor kept column 5 on the shorter line: end of "first"
  TEST_ASSERT_EQUAL_STRING("firstX\nsecond", ta.utf8().c_str());
  TEST_ASSERT_TRUE(ta.down(40));
  ta.insert(U'Y');
  TEST_ASSERT_EQUAL_STRING("firstX\nsecondY", ta.utf8().c_str());

  // Wrapped lines count as visual lines.
  TextArea w;
  typeInto(w, U"abcdefghij");
  TEST_ASSERT_TRUE(w.up(4));  // "abcd|efgh|ij": from line 3 to line 2
  w.insert(U'Z');
  TEST_ASSERT_EQUAL_STRING("abcdefZghij", w.utf8().c_str());
}

static KeyEvent overlayKe(Action a) {
  KeyEvent k;
  k.action = a;
  return k;
}

void test_overlay_confirm_defaults_cancel_and_wraps() {
  Overlay o;
  o.confirm(U"Log out?", U"Log out");
  TEST_ASSERT_TRUE(o.isOpen());
  TEST_ASSERT_EQUAL(0, o.selected());
  TEST_ASSERT_EQUAL(int(Overlay::Result::None), int(o.key(overlayKe(Action::Up))));
  TEST_ASSERT_EQUAL(1, o.selected());
  TEST_ASSERT_EQUAL(int(Overlay::Result::Picked), int(o.key(overlayKe(Action::Select))));
  TEST_ASSERT_FALSE(o.isOpen());
  TEST_ASSERT_EQUAL(1, o.selected());
}

void test_overlay_back_and_left_close() {
  Overlay o;
  o.confirm(U"Delete?", U"Delete");
  TEST_ASSERT_EQUAL(int(Overlay::Result::Closed), int(o.key(overlayKe(Action::Back))));
  TEST_ASSERT_FALSE(o.isOpen());
  o.confirm(U"Delete?", U"Delete");
  TEST_ASSERT_EQUAL(int(Overlay::Result::Closed), int(o.key(overlayKe(Action::Left))));
  TEST_ASSERT_FALSE(o.isOpen());
}

void test_overlay_activate_picks() {
  Overlay o;
  o.open(U"menu", {ListItem{U"a", U""}, ListItem{U"b", U""}, ListItem{U"c", U""}});
  KeyEvent k;
  k.action = Action::Activate;
  k.index = 2;
  k.target = static_cast<uint8_t>(ListId::Menu);
  k.then = Action::Select;
  TEST_ASSERT_EQUAL(int(Overlay::Result::Picked), int(o.key(k)));
  TEST_ASSERT_EQUAL(2, o.selected());
  TEST_ASSERT_FALSE(o.isOpen());
}

void test_overlay_activate_out_of_range() {
  Overlay o;
  o.open(U"menu", {ListItem{U"a", U""}, ListItem{U"b", U""}, ListItem{U"c", U""}});
  o.key(overlayKe(Action::Down));  // selected 1
  TEST_ASSERT_EQUAL(1, o.selected());
  KeyEvent k;
  k.action = Action::Activate;
  k.index = 9;  // past the end
  k.target = static_cast<uint8_t>(ListId::Menu);
  k.then = Action::Select;
  TEST_ASSERT_EQUAL(int(Overlay::Result::None), int(o.key(k)));
  TEST_ASSERT_TRUE(o.isOpen());
  TEST_ASSERT_EQUAL(1, o.selected());
  KeyEvent other;
  other.action = Action::Activate;
  other.index = 1;
  other.target = static_cast<uint8_t>(ListId::Main);  // another list's row
  other.then = Action::Select;
  TEST_ASSERT_EQUAL(int(Overlay::Result::None), int(o.key(other)));
  TEST_ASSERT_TRUE(o.isOpen());
  TEST_ASSERT_EQUAL(1, o.selected());
}

void test_overlay_render_bottom_right() {
  Grid g(53, 20);
  Overlay o;
  o.open(U"menu", {ListItem{U"a", U""}, ListItem{U"b", U""}, ListItem{U"c", U""}});
  o.render(g, 1, 18, 0, OverlayPlace::BottomRight, T());
  // width 30, left 22, title bar on row 14 across cols 22..51.
  TEST_ASSERT_EQUAL_HEX16(T().barBg, g.at(22, 14).bg);
  TEST_ASSERT_EQUAL_HEX16(T().barBg, g.at(51, 14).bg);
  TEST_ASSERT_EQUAL_STRING(" menu", rowText(g, 14, 22, 27).c_str());
  // Rows 15..17 hold a, b, c (label at left + 2 = 24); row 15 is selected.
  TEST_ASSERT_EQUAL_HEX32(U'a', g.at(24, 15).cp);
  TEST_ASSERT_EQUAL_HEX32(U'b', g.at(24, 16).cp);
  TEST_ASSERT_EQUAL_HEX32(U'c', g.at(24, 17).cp);
  TEST_ASSERT_EQUAL(kEdgeTop | kEdgeBottom | kEdgeLeft, g.at(22, 15).edges);
  TEST_ASSERT_EQUAL(kEdgeTop | kEdgeBottom | kEdgeRight, g.at(51, 15).edges);
}

void test_overlay_render_centre_and_bottom() {
  Grid g(40, 20);
  Overlay o;
  o.open(U"Discard draft?", {ListItem{U"Cancel", U""}, ListItem{U"Discard", U"", ItemTone::Error}});
  o.render(g, 0, 20, 0, OverlayPlace::Centre, T());
  // width max(20, 14+2)=20, left (40-20)/2=10, boxTop (20-2-1)/2=8.
  TEST_ASSERT_EQUAL_HEX16(T().barBg, g.at(10, 8).bg);
  TEST_ASSERT_EQUAL_HEX32(U'D', g.at(11, 8).cp);
  TEST_ASSERT_EQUAL_HEX32(U'C', g.at(12, 9).cp);  // "Cancel" label at left + 2 = 12

  Grid g2(40, 20);
  Overlay c;
  c.confirm(U"Log out?", U"Log out");
  c.render(g2, 1, 19, 0, OverlayPlace::Bottom, T());
  // Title in error at col 1, row bottom-3=16; list in [17,19) at cols 1..38.
  TEST_ASSERT_EQUAL_HEX32(U'L', g2.at(1, 16).cp);
  TEST_ASSERT_EQUAL_HEX16(T().error, g2.at(1, 16).fg);
  TEST_ASSERT_EQUAL_HEX32(U'C', g2.at(3, 17).cp);     // "Cancel" label at col 1 + 2 = 3
  TEST_ASSERT_EQUAL_HEX16(T().error, g2.at(3, 18).fg);  // the action row is error-toned
}

void test_text_area_render_cursor_visible() {
  TextArea ta;
  for (int i = 0; i < 10; ++i) {
    ta.insert(static_cast<char32_t>(U'0' + i));
    if (i < 9) ta.insert(U'\n');
  }
  Grid g(10, 5);
  ta.render(g, 1, 4, T(), true);  // 3 rows: lines 7, 8, 9
  TEST_ASSERT_EQUAL_HEX32(U'7', g.at(0, 1).cp);
  TEST_ASSERT_EQUAL_HEX32(U'9', g.at(0, 3).cp);
  TEST_ASSERT_EQUAL_HEX16(T().cursorBg, g.at(1, 3).bg);
  Grid g2(10, 5);
  ta.render(g2, 1, 4, T(), false);
  TEST_ASSERT_TRUE(g2.at(1, 3).bg != T().cursorBg);
}

static TapMap makeTaps() {
  TapMap m;
  m.setOrigin({1, 1}, 0);
  return m;
}

void test_list_registers_rows() {
  ListView v;
  std::vector<ListItem> items;
  for (int i = 0; i < 5; ++i) items.push_back(ListItem{U"x", U""});
  v.setItems(items);
  v.setSelected(0);
  TapMap m = makeTaps();
  setActiveTaps(&m);
  Grid g(10, 7);
  v.render(g, 1, 6, 0, 10, T());  // rows 1..5 show items 0..4
  Target t = m.hit(0, 4);         // the fourth drawn row
  TEST_ASSERT_EQUAL(int(TargetKind::Row), int(t.kind));
  TEST_ASSERT_EQUAL(int(ListId::Main), int(t.list));
  TEST_ASSERT_EQUAL(3, t.index);
  const ListInfo* li = m.list(ListId::Main);
  TEST_ASSERT_NOT_NULL(li);
  TEST_ASSERT_EQUAL(0, li->selected);
  TEST_ASSERT_EQUAL(5, li->count);
  TEST_ASSERT_TRUE(li->wraps);

  ListView v2;
  std::vector<ListItem> items2;
  for (int i = 0; i < 10; ++i) items2.push_back(ListItem{U"y", U""});
  v2.setItems(items2);
  v2.setSelected(7);
  TapMap m2 = makeTaps();
  setActiveTaps(&m2);
  Grid g2(10, 8);
  v2.render(g2, 2, 7, 0, 10, T());  // height 5, first shown is 3
  TEST_ASSERT_EQUAL(3, m2.hit(0, 2).index);
  const ListInfo* li2 = m2.list(ListId::Main);
  TEST_ASSERT_EQUAL(7, li2->selected);
  TEST_ASSERT_EQUAL(10, li2->count);
}

void test_list_select_out_of_range_refused() {
  ListView v;
  v.setItems({ListItem{U"a", U""}, ListItem{U"b", U""}, ListItem{U"c", U""}});
  v.setSelected(1);
  TEST_ASSERT_FALSE(v.select(10));
  TEST_ASSERT_FALSE(v.select(-1));
  TEST_ASSERT_EQUAL(1, v.selected());
  TEST_ASSERT_TRUE(v.select(2));
  TEST_ASSERT_EQUAL(2, v.selected());
}

void test_entry_list_row_spans_two_rows() {
  EntryList el;
  el.setRows(feedRows(3));
  TapMap m = makeTaps();
  setActiveTaps(&m);
  Grid g(20, 8);
  el.render(g, 0, 8, T());  // entry 1 at rows 2..3
  TEST_ASSERT_EQUAL(1, m.hit(0, 2).index);
  Target t = m.hit(0, 3);
  TEST_ASSERT_EQUAL(int(TargetKind::Row), int(t.kind));
  TEST_ASSERT_EQUAL(int(ListId::Main), int(t.list));
  TEST_ASSERT_EQUAL(1, t.index);
  const ListInfo* li = m.list(ListId::Main);
  TEST_ASSERT_NOT_NULL(li);
  TEST_ASSERT_EQUAL(3, li->count);
  TEST_ASSERT_TRUE(li->wraps);
}

void test_drawer_registers_people() {
  PresenceEntry a, b;
  a.username = "rain";
  b.username = "tardibyte";
  Drawer d;
  d.setPeople({a, b}, 0, 300000);
  TapMap m = makeTaps();
  setActiveTaps(&m);
  Grid g(40, 6);
  d.render(g, 1, 6, T());  // people at rows 2, 3
  int x = 40 - Drawer::width(40);
  Target t = m.hit(x + 1, 3);
  TEST_ASSERT_EQUAL(int(TargetKind::Row), int(t.kind));
  TEST_ASSERT_EQUAL(int(ListId::Drawer), int(t.list));
  TEST_ASSERT_EQUAL(1, t.index);
  TEST_ASSERT_TRUE(d.select(1));
  TEST_ASSERT_EQUAL_STRING("tardibyte", d.selectedUsername().c_str());
  TEST_ASSERT_FALSE(d.select(5));
}

void test_log_registers_messages() {
  LogView v;
  v.setLines({line("a", "a1"), line("a", "a2"), line("x", "-", true), line("b", "b1")});
  TEST_ASSERT_EQUAL(2, v.messageCount());
  TapMap m = makeTaps();
  setActiveTaps(&m);
  Grid g(10, 6);
  v.render(g, 0, 4, 0, T());  // following: rows a1, a2, divider, b1
  TEST_ASSERT_EQUAL(0, m.hit(0, 0).index);
  TEST_ASSERT_EQUAL(0, m.hit(0, 1).index);
  TEST_ASSERT_EQUAL(int(TargetKind::None), int(m.hit(0, 2).kind));  // divider
  Target tb = m.hit(0, 3);
  TEST_ASSERT_EQUAL(int(TargetKind::Row), int(tb.kind));
  TEST_ASSERT_EQUAL(int(ListId::Log), int(tb.list));
  TEST_ASSERT_EQUAL(1, tb.index);
  const ListInfo* li = m.list(ListId::Log);
  TEST_ASSERT_NOT_NULL(li);
  TEST_ASSERT_EQUAL(-1, li->selected);
  TEST_ASSERT_EQUAL(2, li->count);
  TEST_ASSERT_FALSE(li->wraps);
  TEST_ASSERT_TRUE(v.selectMessage(1));
  TEST_ASSERT_EQUAL_STRING("b", v.selectedId().c_str());
  TEST_ASSERT_FALSE(v.selectMessage(2));
}

void test_input_field_hit_scrolled() {
  InputLine in;
  in.setFieldId(7);
  in.setText(std::u32string(60, U'x'));  // cursor at end
  TapMap m = makeTaps();
  setActiveTaps(&m);
  Grid g(40, 2);
  in.render(g, 0, T());  // prompt "> ", avail 38, first = 23
  Target t = m.hit(2, 0);
  TEST_ASSERT_EQUAL(int(TargetKind::Field), int(t.kind));
  TEST_ASSERT_EQUAL(7, t.field);
  TEST_ASSERT_EQUAL(23, t.index);
  // A disabled line still registers, because a tap focuses it.
  in.setDisabled(true);
  TapMap m2 = makeTaps();
  setActiveTaps(&m2);
  in.render(g, 0, T());
  TEST_ASSERT_EQUAL(int(TargetKind::Field), int(m2.hit(2, 0).kind));
  // setCursor clamps to the text length.
  in.setCursor(999);
  TEST_ASSERT_EQUAL(60, in.cursor());
  in.setCursor(5);
  TEST_ASSERT_EQUAL(5, in.cursor());
}

void test_input_emoji_button_on_touch() {
  InputLine in;
  in.setEmojiButton(true);
  in.insert(U'a');
  Grid g(20, 1);
  setActiveTaps(nullptr);  // no touch: no button
  in.render(g, 0, T());
  TEST_ASSERT_EQUAL(int(U'>'), int(g.at(0, 0).cp));
  TapMap m = makeTaps();
  setActiveTaps(&m);
  in.render(g, 0, T());
  TEST_ASSERT_EQUAL(int(kTrayEmoji[0]), int(g.at(0, 0).cp));
  TEST_ASSERT_EQUAL(int(wideTail(kTrayEmoji[0])), int(g.at(1, 0).cp));
  TEST_ASSERT_EQUAL(int(U'>'), int(g.at(3, 0).cp));
  TEST_ASSERT_EQUAL(int(U'a'), int(g.at(5, 0).cp));
  Target b = m.hit(1, 0);
  TEST_ASSERT_EQUAL(int(TargetKind::Fixed), int(b.kind));
  TEST_ASSERT_EQUAL(int(Action::Emoji), int(b.tap.action));
  TEST_ASSERT_EQUAL(int(TargetKind::Field), int(m.hit(5, 0).kind));
  setActiveTaps(nullptr);
}

void test_emoji_tray_taps() {
  SymbolTable s;
  s.open(U"", SymbolTable::Set::Emoji);
  TapMap m = makeTaps();
  setActiveTaps(&m);
  Grid g(40, 30);
  g.fill(T().bg);
  s.render(g, g.rows() - 1, T());
  setActiveTaps(nullptr);
  int c3 = -1, r3 = -1;
  for (int r = 0; r < g.rows() && c3 < 0; ++r)
    for (int c = 0; c < g.cols(); ++c)
      if (g.at(c, r).cp == SymbolTable::emoji()[3]) { c3 = c; r3 = r; break; }
  Target t = m.hit(c3 + 1, r3);
  TEST_ASSERT_EQUAL(int(TargetKind::Fixed), int(t.kind));
  auto o = s.key(t.tap);  // a tap picks that emoji, and the tray stays open
  TEST_ASSERT_EQUAL(int(SymbolTable::Kind::Insert), int(o.kind));
  TEST_ASSERT_EQUAL(int(SymbolTable::emoji()[3]), int(o.ch));
  TEST_ASSERT_TRUE(s.isOpen());
  // A tap outside the tray closes it.
  Target out = m.hit(0, 0);
  TEST_ASSERT_EQUAL(int(Action::Back), int(out.tap.action));
  TEST_ASSERT_EQUAL(int(SymbolTable::Kind::Closed), int(s.key(out.tap).kind));
}

static KeyEvent ch(char32_t c) {
  KeyEvent k;
  k.action = Action::Char;
  k.ch = c;
  return k;
}

void test_symbol_table_backspace_closes() {
  SymbolTable s;
  s.setBackspaceCloses(true);
  for (auto set : {SymbolTable::Set::Symbols, SymbolTable::Set::Emoji}) {
    s.open(U"", set, true);
    TEST_ASSERT_EQUAL(int(SymbolTable::Kind::Closed), int(s.key(KeyEvent{Action::Backspace}).kind));
    TEST_ASSERT_FALSE(s.isOpen());
  }
  // Without it, the emoji tray swallows Backspace.
  s.setBackspaceCloses(false);
  s.open(U"", SymbolTable::Set::Emoji);
  s.key(KeyEvent{Action::Backspace});
  TEST_ASSERT_TRUE(s.isOpen());
}

void test_type_filter() {
  TypeFilter f;
  TEST_ASSERT_FALSE(f.active());
  TEST_ASSERT_TRUE(f.matches(U"#general"));
  // Back and Backspace pass through while nothing is typed.
  TEST_ASSERT_FALSE(f.key(KeyEvent{Action::Back}));
  TEST_ASSERT_FALSE(f.key(KeyEvent{Action::Backspace}));
  TEST_ASSERT_FALSE(f.key(KeyEvent{Action::Down}));
  TEST_ASSERT_TRUE(f.key(ch(U'G')));
  TEST_ASSERT_TRUE(f.key(ch(U'e')));
  TEST_ASSERT_TRUE(f.active());
  TEST_ASSERT_TRUE(f.matches(U"general"));    // case-insensitive
  TEST_ASSERT_TRUE(f.matches(U"BigEvents"));  // anywhere in the name
  TEST_ASSERT_FALSE(f.matches(U"random"));
  TEST_ASSERT_TRUE(f.key(KeyEvent{Action::Backspace}));
  TEST_ASSERT_EQUAL(1, int(f.text().size()));
  TEST_ASSERT_TRUE(f.key(KeyEvent{Action::Back}));  // Back clears it all
  TEST_ASSERT_FALSE(f.active());
  // A leading space is ignored; control characters never count.
  TEST_ASSERT_FALSE(f.key(ch(U' ')));
  TEST_ASSERT_FALSE(f.key(ch(U'\n')));
  TEST_ASSERT_FALSE(f.active());
}

void test_text_area_field_rows() {
  TextArea ta;
  ta.setFieldId(3);
  for (char32_t c : std::u32string(U"ab\ncd")) ta.insert(c);
  TapMap m = makeTaps();
  setActiveTaps(&m);
  Grid g(10, 5);
  ta.render(g, 1, 4, T(), true);  // row 1: seg {0,2}; row 2: seg {3,2}
  Target t = m.hit(1, 2);
  TEST_ASSERT_EQUAL(int(TargetKind::Field), int(t.kind));
  TEST_ASSERT_EQUAL(3, t.field);
  TEST_ASSERT_EQUAL(4, t.index);
  TEST_ASSERT_EQUAL(2, m.hit(5, 1).index);  // past the end of row 1 caps at 2
  ta.setCursor(999);
  TEST_ASSERT_EQUAL(5, ta.length());
}

void test_status_bar_touch_layout() {
  Grid g(40, 2);
  TapMap m = makeTaps();
  setActiveTaps(&m);
  renderStatusBar(g, T(), U"#general", 12, U'\u25cf', 3, 87, "14:02",
                  StatusTouch{true, true, true});
  TEST_ASSERT_EQUAL_HEX32(0x2039, g.at(0, 0).cp);                     // ‹
  TEST_ASSERT_EQUAL_STRING("#general", rowText(g, 0, 2, 10).c_str());  // title from col 2
  TEST_ASSERT_EQUAL_HEX32(0x22EF, g.at(38, 0).cp);                    // ⋯ before the trailing space
  TEST_ASSERT_EQUAL_STRING("14:02", rowText(g, 0, 32, 37).c_str());    // the clock keeps every digit
  Target back = m.hit(1, 0);
  TEST_ASSERT_EQUAL(int(TargetKind::Fixed), int(back.kind));
  TEST_ASSERT_EQUAL(int(Action::Back), int(back.tap.action));
  TEST_ASSERT_EQUAL(int(Action::OpenHome), int(back.hold.action));
  Target menu = m.hit(38, 0);
  TEST_ASSERT_EQUAL(int(TargetKind::Fixed), int(menu.kind));
  TEST_ASSERT_EQUAL(int(Action::Palette), int(menu.tap.action));

  Grid g2(40, 2);
  TapMap m2 = makeTaps();
  setActiveTaps(&m2);
  renderStatusBar(g2, T(), U"#general", 12, U'\u25cf', 3, 87, "14:02",
                  StatusTouch{true, false, false});
  TEST_ASSERT_NOT_EQUAL(0x2039, g2.at(0, 0).cp);   // no back arrow
  TEST_ASSERT_NOT_EQUAL(0x22EF, g2.at(38, 0).cp);  // no menu dots
  TEST_ASSERT_EQUAL(int(TargetKind::None), int(m2.hit(1, 0).kind));
}

void test_status_bar_header() {
  // A header row fits fewer, wider cells: the right side ends at its last one.
  Grid g(40, 2);
  g.setHeader(HeaderSize{20, 32, 2});
  TapMap m = makeTaps();
  setActiveTaps(&m);
  renderStatusBar(g, T(), U"#general", 12, U'\u25cf', 3, 87, "14:02",
                  StatusTouch{true, true, true});
  TEST_ASSERT_EQUAL_HEX32(0x22EF, g.at(30, 0).cp);
  TEST_ASSERT_EQUAL_STRING("14:02", rowText(g, 0, 24, 29).c_str());
  TEST_ASSERT_EQUAL(int(Action::Palette), int(m.hit(62, 0).tap.action));  // cells 2 px wide
  TEST_ASSERT_EQUAL(int(Action::Back), int(m.hit(7, 0).tap.action));
}

void test_overlay_registers_taps() {
  Overlay o;
  o.open(U"menu", {ListItem{U"a", U""}, ListItem{U"b", U""}, ListItem{U"c", U""}});
  TapMap m = makeTaps();
  setActiveTaps(&m);
  Grid g(40, 20);
  o.render(g, 0, 20, 0, OverlayPlace::Centre, T());  // items at rows 9, 10, 11
  Target in = m.hit(12, 10);
  TEST_ASSERT_EQUAL(int(TargetKind::Row), int(in.kind));
  TEST_ASSERT_EQUAL(int(ListId::Menu), int(in.list));
  TEST_ASSERT_EQUAL(1, in.index);
  Target out = m.hit(0, 0);  // outside the box closes it
  TEST_ASSERT_EQUAL(int(TargetKind::Fixed), int(out.kind));
  TEST_ASSERT_EQUAL(int(Action::Back), int(out.tap.action));
}

void test_banner_tap_target() {
  Banner b;
  b.show(U"hi", 1000, 2000, false);
  TapMap m = makeTaps();
  setActiveTaps(&m);
  Grid g(20, 3);
  b.render(g, 2, T(), 1500);  // visible
  TEST_ASSERT_EQUAL(int(TargetKind::Banner), int(m.hit(0, 2).kind));
  TapMap m2 = makeTaps();
  setActiveTaps(&m2);
  b.render(g, 2, T(), 4000);  // expired
  TEST_ASSERT_EQUAL(int(TargetKind::None), int(m2.hit(0, 2).kind));
}

void test_no_registration_without_active_map() {
  setActiveTaps(nullptr);
  ListView v;
  v.setItems({ListItem{U"a", U""}});
  Grid g(10, 3);
  v.render(g, 0, 3, 0, 10, T());  // must not crash
  TEST_ASSERT_NULL(activeTaps());
}

// Every row's height in px under the layout, rows 1..rows-2 (tile rows rowH).
int layoutPx(const HomeLayout& l, int rowH) {
  int px = 0;
  for (int r = 1; r <= l.lastRow; ++r) {
    int h = rowH;
    for (const auto& p : l.pads)
      if (p.first == r) h = p.second;
    px += h;
  }
  return px;
}

void test_home_layout_fits() {
  // Panel width, grid rows, cell (tile rows' font): landscape and portrait,
  // both text sizes, Big cells.
  struct Case {
    int panelW, rows, rowH;
    CellSize cell;
  } cases[] = {{320, 15, 16, {10, 18}}, {320, 20, 12, {8, 14}},
               {240, 20, 16, {10, 18}}, {240, 26, 12, {8, 14}}};
  for (const Case& c : cases) {
    const int areaPx = (c.rows - 2) * c.rowH;
    HomeLayout l = homeLayout(c.panelW, areaPx, c.rows, c.cell, 6, 90);
    TEST_ASSERT_EQUAL(6, l.tiles.size());
    TEST_ASSERT_TRUE(l.lastRow <= c.rows - 2);
    const int used = layoutPx(l, c.cell.h);
    TEST_ASSERT_TRUE(used <= areaPx);
    // Centred: the pad above (row 1) and the space below differ by at most 1.
    TEST_ASSERT_EQUAL(1, l.pads[0].first);
    const int below = areaPx - used;
    TEST_ASSERT_TRUE(below - l.pads[0].second >= 0 && below - l.pads[0].second <= 1);
    // Centred across, to within one px.
    const int width = (l.tiles[1].c1 + 1) * c.cell.w;
    TEST_ASSERT_TRUE(std::abs((c.panelW - width - l.shiftPx) - l.shiftPx) <= 1);
    for (const TileRect& r : l.tiles) {
      TEST_ASSERT_TRUE(r.r0 >= 2 && r.r1 <= l.lastRow);
      TEST_ASSERT_TRUE(r.r1 - r.r0 + 1 >= 3);
      TEST_ASSERT_TRUE(r.c1 - r.c0 + 1 >= 10);
      for (const auto& p : l.pads) TEST_ASSERT_TRUE(p.first < r.r0 || p.first > r.r1);
    }
    // A blank column between the two columns, a pad row between tile rows,
    // equal sizes, row-major.
    for (int i = 0; i < 6; i += 2) {
      TEST_ASSERT_EQUAL(l.tiles[i].c1 + 2, l.tiles[i + 1].c0);
      TEST_ASSERT_EQUAL(l.tiles[i].r0, l.tiles[i + 1].r0);
      TEST_ASSERT_EQUAL(l.tiles[0].c1 - l.tiles[0].c0, l.tiles[i + 1].c1 - l.tiles[i + 1].c0);
    }
    for (int i = 0; i + 2 < 6; i += 2) TEST_ASSERT_EQUAL(l.tiles[i].r1 + 2, l.tiles[i + 2].r0);
  }
}

void test_home_layout_landscape() {
  // T-Deck/Wio landscape, Comfortable: 208 px between bar and hint, 10x18 cells.
  HomeLayout l = homeLayout(320, 208, 15, CellSize{10, 18}, 6, 90);
  TEST_ASSERT_EQUAL(15, l.shiftPx);  // 29 cells across
  TEST_ASSERT_EQUAL(0, l.tiles[0].c0);
  TEST_ASSERT_EQUAL(13, l.tiles[0].c1);
  TEST_ASSERT_EQUAL(15, l.tiles[1].c0);
  TEST_ASSERT_EQUAL(28, l.tiles[1].c1);
  TEST_ASSERT_EQUAL(2, l.tiles[0].r0);  // 3 rows each, gaps at rows 5 and 9
  TEST_ASSERT_EQUAL(4, l.tiles[0].r1);
  TEST_ASSERT_EQUAL(6, l.tiles[2].r0);
  TEST_ASSERT_EQUAL(12, l.tiles[5].r1);
  TEST_ASSERT_EQUAL(12, l.lastRow);
  TEST_ASSERT_EQUAL(3, l.pads.size());
  TEST_ASSERT_EQUAL(13, l.pads[0].second);  // (208 - 162 - 20) / 2
  TEST_ASSERT_EQUAL(5, l.pads[1].first);
  TEST_ASSERT_EQUAL(10, l.pads[1].second);  // a gap as wide as a cell
}

void test_home_layout_too_small() {
  TEST_ASSERT_EQUAL(0, homeLayout(160, 100, 8, CellSize{10, 18}, 6, 90).tiles.size());
}

void test_tile_span_centre() {
  // 208 px between the status bar and the hint, 18 px rows, 90%: 187 px is
  // 10 rows (180 px), leaving 14 px above and 14 below.
  TileSpan r = tileSpan(208, 18, 90);
  TEST_ASSERT_EQUAL(10, r.cells);
  TEST_ASSERT_EQUAL(14, r.padPx);
  // An odd leftover puts the extra pixel after.
  r = tileSpan(209, 18, 90);
  TEST_ASSERT_EQUAL(10, r.cells);
  TEST_ASSERT_EQUAL(14, r.padPx);
  // Never more cells than fit.
  r = tileSpan(36, 18, 100);
  TEST_ASSERT_EQUAL(2, r.cells);
  TEST_ASSERT_EQUAL(0, r.padPx);
  // Across: 320 px of 10 px cells at 90% is 29 cells, 15 px each side.
  r = tileSpan(320, 10, 90);
  TEST_ASSERT_EQUAL(29, r.cells);
  TEST_ASSERT_EQUAL(15, r.padPx);
}

void test_tile_move() {
  TEST_ASSERT_EQUAL(2, tileMove(0, 6, Action::Down));
  TEST_ASSERT_EQUAL(4, tileMove(4, 6, Action::Down));
  TEST_ASSERT_EQUAL(0, tileMove(1, 6, Action::Left));
  TEST_ASSERT_EQUAL(0, tileMove(0, 6, Action::Left));
  TEST_ASSERT_EQUAL(1, tileMove(0, 6, Action::Right));
  TEST_ASSERT_EQUAL(1, tileMove(1, 6, Action::Right));
  TEST_ASSERT_EQUAL(1, tileMove(3, 6, Action::Up));
  TEST_ASSERT_EQUAL(1, tileMove(1, 6, Action::Up));
}


static KeyEvent K(Action a) {
  KeyEvent k;
  k.action = a;
  return k;
}

void test_symbols_pick_inserts_and_closes() {
  SymbolTable s;
  s.open(U"");
  TEST_ASSERT_TRUE(s.isOpen());
  auto o = s.key(K(Action::Select));
  TEST_ASSERT_EQUAL(int(SymbolTable::Kind::Insert), int(o.kind));
  TEST_ASSERT_EQUAL(int(SymbolTable::symbols()[0]), int(o.ch));
  TEST_ASSERT_FALSE(s.isOpen());
}

void test_symbols_move_wraps_in_two_dimensions() {
  SymbolTable s;
  s.open(U"");
  const int cols = SymbolTable::kCols, n = int(SymbolTable::symbols().size());
  s.key(K(Action::Left));  // wraps to the last symbol
  TEST_ASSERT_EQUAL(n - 1, s.selected());
  s.key(K(Action::Right));
  TEST_ASSERT_EQUAL(0, s.selected());
  s.key(K(Action::Down));
  TEST_ASSERT_EQUAL(cols, s.selected());
  s.key(K(Action::Up));
  s.key(K(Action::Up));  // from the top row to the bottom row, same column
  TEST_ASSERT_EQUAL(n - cols, s.selected());
}

void test_symbols_commands_row() {
  SymbolTable s;
  s.open(U"Commands\u2026");
  const int n = int(SymbolTable::symbols().size());
  s.key(K(Action::Up));  // top row up lands on the commands row
  TEST_ASSERT_EQUAL(n, s.selected());
  s.key(K(Action::Down));  // and down from it wraps to the top
  TEST_ASSERT_EQUAL(0, s.selected());
  s.key(K(Action::Up));
  auto o = s.key(K(Action::Select));
  TEST_ASSERT_EQUAL(int(SymbolTable::Kind::Commands), int(o.kind));
  TEST_ASSERT_FALSE(s.isOpen());
}

void test_symbols_repeat_ignored_back_closes_other_passes() {
  SymbolTable s;
  s.open(U"");
  s.key(K(Action::Right));
  // Symbols again switches to the emoji tray, at its first emoji.
  TEST_ASSERT_EQUAL(int(SymbolTable::Kind::None), int(s.key(K(Action::Symbols)).kind));
  TEST_ASSERT_TRUE(s.isOpen());
  TEST_ASSERT_EQUAL(int(SymbolTable::Set::Emoji), int(s.set()));
  TEST_ASSERT_EQUAL(0, s.selected());
  // There a further Symbols changes nothing.
  s.key(K(Action::Right));
  s.key(K(Action::Symbols));
  TEST_ASSERT_EQUAL(int(SymbolTable::Set::Emoji), int(s.set()));
  TEST_ASSERT_EQUAL(1, s.selected());
  TEST_ASSERT_EQUAL(int(SymbolTable::Kind::Closed), int(s.key(K(Action::Back)).kind));
  TEST_ASSERT_FALSE(s.isOpen());
  s.open(U"");
  TEST_ASSERT_EQUAL(0, s.selected());  // each opening starts at the first symbol
  KeyEvent c = K(Action::Char);
  c.ch = U'a';
  TEST_ASSERT_EQUAL(int(SymbolTable::Kind::PassThrough), int(s.key(c).kind));
  TEST_ASSERT_FALSE(s.isOpen());
}

void test_symbols_render_highlights_selection() {
  Grid g(40, 13);
  g.fill(T().bg);
  SymbolTable s;
  s.open(U"Commands\u2026");
  s.render(g, g.rows() - 1, T());
  bool found = false;
  for (int r = 0; r < g.rows() && !found; ++r)
    for (int c = 0; c < g.cols(); ++c)
      if (g.at(c, r).cp == SymbolTable::symbols()[0] && g.at(c, r).bg == T().selectBg) found = true;
  TEST_ASSERT_TRUE(found);
}

void test_emoji_tray_inserts_and_stays_open() {
  SymbolTable s;
  s.open(U"", SymbolTable::Set::Emoji);
  TEST_ASSERT_EQUAL(40, int(SymbolTable::emoji().size()));
  s.key(K(Action::Right));
  auto o = s.key(K(Action::Select));
  TEST_ASSERT_EQUAL(int(SymbolTable::Kind::Insert), int(o.kind));
  TEST_ASSERT_EQUAL(int(SymbolTable::emoji()[1]), int(o.ch));
  TEST_ASSERT_TRUE(s.isOpen());  // camillia-mt's tray stays open for more
  // Repeats of its key and other keys are swallowed; only Back closes it.
  TEST_ASSERT_EQUAL(int(SymbolTable::Kind::None), int(s.key(K(Action::Emoji)).kind));
  KeyEvent c = K(Action::Char);
  c.ch = U'a';
  TEST_ASSERT_EQUAL(int(SymbolTable::Kind::None), int(s.key(c).kind));
  TEST_ASSERT_TRUE(s.isOpen());
  TEST_ASSERT_EQUAL(1, s.selected());
  TEST_ASSERT_EQUAL(int(SymbolTable::Kind::Closed), int(s.key(K(Action::Back)).kind));
  TEST_ASSERT_FALSE(s.isOpen());
}

void test_emoji_tray_grid_and_commands_row() {
  SymbolTable s;
  s.open(U"Commands\u2026", SymbolTable::Set::Emoji);
  TEST_ASSERT_EQUAL(SymbolTable::kEmojiCols, s.cols());
  s.key(K(Action::Down));
  TEST_ASSERT_EQUAL(SymbolTable::kEmojiCols, s.selected());
  s.key(K(Action::Up));
  s.key(K(Action::Up));  // the top row up lands on the commands row
  TEST_ASSERT_EQUAL(int(SymbolTable::emoji().size()), s.selected());
  TEST_ASSERT_EQUAL(int(SymbolTable::Kind::Commands), int(s.key(K(Action::Select)).kind));
  TEST_ASSERT_FALSE(s.isOpen());
}

void test_emoji_tray_spaced_out() {
  Grid g(40, 30);
  g.fill(T().bg);
  SymbolTable s;
  s.open(U"", SymbolTable::Set::Emoji);
  s.render(g, g.rows() - 1, T());
  int c0 = -1, r0 = -1;
  for (int r = 0; r < g.rows() && c0 < 0; ++r)
    for (int c = 0; c < g.cols(); ++c)
      if (g.at(c, r).cp == SymbolTable::emoji()[0]) { c0 = c; r0 = r; break; }
  TEST_ASSERT_TRUE(c0 >= 0);
  // One blank cell between neighbours, and a blank row between rows.
  TEST_ASSERT_EQUAL(int(SymbolTable::emoji()[1]), int(g.at(c0 + 3, r0).cp));
  TEST_ASSERT_EQUAL(int(U' '), int(g.at(c0 + 2, r0).cp));
  TEST_ASSERT_EQUAL(int(U' '), int(g.at(c0, r0 + 1).cp));
  TEST_ASSERT_EQUAL(int(SymbolTable::emoji()[SymbolTable::kEmojiCols]), int(g.at(c0, r0 + 2).cp));
  // The commands row sits below the last emoji row, apart from it.
  Grid k(40, 30);
  k.fill(T().bg);
  s.open(U"cmds", SymbolTable::Set::Emoji);
  s.render(k, k.rows() - 1, T());
  int lastRow = -1, cmdRow = -1;
  for (int r = 0; r < k.rows(); ++r)
    for (int c = 0; c < k.cols(); ++c) {
      if (k.at(c, r).cp == SymbolTable::emoji().back()) lastRow = r;
      if (k.at(c, r).cp == U'c' && k.at(c + 1, r).cp == U'm') cmdRow = r;
    }
  TEST_ASSERT_EQUAL(lastRow + 2, cmdRow);
  // A shorter screen puts more on a row to keep the blank rows.
  Grid m(40, 13);
  m.fill(T().bg);
  s.open(U"", SymbolTable::Set::Emoji);
  s.render(m, m.rows() - 1, T());
  TEST_ASSERT_EQUAL(8, s.cols());
  int mc = -1, mr = -1;
  for (int r = 0; r < m.rows() && mc < 0; ++r)
    for (int c = 0; c < m.cols(); ++c)
      if (m.at(c, r).cp == SymbolTable::emoji()[0]) { mc = c; mr = r; break; }
  TEST_ASSERT_EQUAL(int(U' '), int(m.at(mc, mr + 1).cp));
  TEST_ASSERT_EQUAL(int(SymbolTable::emoji()[8]), int(m.at(mc, mr + 2).cp));
  // Too short even then: the rows close up instead of clipping.
  Grid h(40, 10);
  h.fill(T().bg);
  s.render(h, h.rows() - 1, T());
  bool last = false;
  for (int r = 0; r < h.rows(); ++r)
    for (int c = 0; c < h.cols(); ++c) last |= h.at(c, r).cp == SymbolTable::emoji().back();
  TEST_ASSERT_TRUE(last);
}

void test_symbols_linear_steps_one_cell_and_wraps() {
  SymbolTable s;
  s.open(U"Commands\u2026", SymbolTable::Set::Symbols, true);
  const int n = int(SymbolTable::symbols().size());
  s.key(K(Action::Down));
  TEST_ASSERT_EQUAL(1, s.selected());
  s.key(K(Action::Up));
  s.key(K(Action::Up));  // wraps backwards onto the commands row
  TEST_ASSERT_EQUAL(n, s.selected());
  s.key(K(Action::Down));
  TEST_ASSERT_EQUAL(0, s.selected());
  // Sym+Enter (Palette) picks while the table is open.
  TEST_ASSERT_EQUAL(int(SymbolTable::Kind::Insert), int(s.key(K(Action::Palette)).kind));
}

void test_emoji_tray_render_draws_both_halves() {
  Grid g(40, 13);
  g.fill(T().bg);
  SymbolTable s;
  s.open(U"", SymbolTable::Set::Emoji);
  s.render(g, g.rows() - 1, T());
  bool found = false;
  for (int r = 0; r < g.rows() && !found; ++r)
    for (int c = 0; c + 1 < g.cols(); ++c)
      if (g.at(c, r).cp == SymbolTable::emoji()[0] && g.at(c + 1, r).cp == wideTail(g.at(c, r).cp) &&
          g.at(c, r).bg == T().selectBg)
        found = true;
  TEST_ASSERT_TRUE(found);
}

void test_put_text_wide_emoji() {
  Grid g(5, 1);
  g.fill(0);
  const char32_t e = kTrayEmoji[0];
  std::u32string s = U"a";
  s += e;
  s += U'\uFE0F';  // a variation selector takes no cell
  s += U"b";
  TEST_ASSERT_EQUAL(4, g.putText(0, 0, s, 1, 0, 5));
  TEST_ASSERT_EQUAL(int(e), int(g.at(1, 0).cp));
  TEST_ASSERT_EQUAL(int(wideTail(e)), int(g.at(2, 0).cp));
  TEST_ASSERT_EQUAL(int(U'b'), int(g.at(3, 0).cp));
  // Half an emoji is never drawn: with one cell left it stops.
  Grid h(2, 1);
  h.fill(0);
  TEST_ASSERT_EQUAL(1, h.putText(0, 0, s, 1, 0, 2));
  TEST_ASSERT_EQUAL(int(U' '), int(h.at(1, 0).cp));
}

void test_input_line_renders_wide_emoji() {
  Grid g(10, 1);
  InputLine in;
  in.insert(kTrayEmoji[0]);
  in.insert(U'x');
  in.render(g, 0, T());
  TEST_ASSERT_EQUAL(int(kTrayEmoji[0]), int(g.at(2, 0).cp));
  TEST_ASSERT_EQUAL(int(wideTail(kTrayEmoji[0])), int(g.at(3, 0).cp));
  TEST_ASSERT_EQUAL(int(U'x'), int(g.at(4, 0).cp));
  TEST_ASSERT_EQUAL(T().cursorBg, g.at(5, 0).bg);  // the cursor sits after x
}

void test_text_area_wraps_by_width() {
  Grid g(4, 3);
  TextArea a(100);
  for (char32_t c : std::u32string(U"abc")) a.insert(c);
  a.insert(kTrayEmoji[0]);  // does not fit after "abc": it starts row 2
  a.render(g, 0, 3, T(), true);
  TEST_ASSERT_EQUAL(int(U'c'), int(g.at(2, 0).cp));
  TEST_ASSERT_EQUAL(int(kTrayEmoji[0]), int(g.at(0, 1).cp));
  TEST_ASSERT_EQUAL(int(wideTail(kTrayEmoji[0])), int(g.at(1, 1).cp));
  TEST_ASSERT_EQUAL(T().cursorBg, g.at(2, 1).bg);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_input_insert_cursor);
  RUN_TEST(test_input_right_at_end_false);
  RUN_TEST(test_input_byte_cap);
  RUN_TEST(test_input_backspace_multibyte);
  RUN_TEST(test_input_hscroll);
  RUN_TEST(test_log_follow_and_select);
  RUN_TEST(test_log_render_follow_and_select);
  RUN_TEST(test_log_outlines_whole_message);
  RUN_TEST(test_grid_outline);
  RUN_TEST(test_cursor_hidden);
  RUN_TEST(test_log_preserves_selection_on_setLines);
  RUN_TEST(test_list_scrolls);
  RUN_TEST(test_list_wraps);
  RUN_TEST(test_list_colours_follow_theme);
  RUN_TEST(test_drawer_wraps);
  RUN_TEST(test_drawer_idle_marker);
  RUN_TEST(test_status_bar_layout);
  RUN_TEST(test_banner_expires);
  RUN_TEST(test_palette_lists_commands);
  RUN_TEST(test_palette_cmail_drops_circ_only);
  RUN_TEST(test_qr_small_url);
  RUN_TEST(test_entry_list_outline);
  RUN_TEST(test_entry_list_scrolls);
  RUN_TEST(test_entry_list_big_heads);
  RUN_TEST(test_entry_list_wraps);
  RUN_TEST(test_text_area_newlines_and_cap);
  RUN_TEST(test_text_area_up_down);
  RUN_TEST(test_overlay_confirm_defaults_cancel_and_wraps);
  RUN_TEST(test_overlay_back_and_left_close);
  RUN_TEST(test_overlay_activate_picks);
  RUN_TEST(test_overlay_activate_out_of_range);
  RUN_TEST(test_overlay_render_bottom_right);
  RUN_TEST(test_overlay_render_centre_and_bottom);
  RUN_TEST(test_text_area_render_cursor_visible);
  RUN_TEST(test_list_registers_rows);
  RUN_TEST(test_list_select_out_of_range_refused);
  RUN_TEST(test_entry_list_row_spans_two_rows);
  RUN_TEST(test_drawer_registers_people);
  RUN_TEST(test_log_registers_messages);
  RUN_TEST(test_input_field_hit_scrolled);
  RUN_TEST(test_text_area_field_rows);
  RUN_TEST(test_status_bar_touch_layout);
  RUN_TEST(test_status_bar_header);
  RUN_TEST(test_overlay_registers_taps);
  RUN_TEST(test_banner_tap_target);
  RUN_TEST(test_no_registration_without_active_map);
  RUN_TEST(test_home_layout_fits);
  RUN_TEST(test_home_layout_landscape);
  RUN_TEST(test_home_layout_too_small);
  RUN_TEST(test_tile_span_centre);
  RUN_TEST(test_tile_move);
  RUN_TEST(test_symbols_pick_inserts_and_closes);
  RUN_TEST(test_symbols_move_wraps_in_two_dimensions);
  RUN_TEST(test_symbols_commands_row);
  RUN_TEST(test_symbols_repeat_ignored_back_closes_other_passes);
  RUN_TEST(test_symbols_render_highlights_selection);
  RUN_TEST(test_emoji_tray_inserts_and_stays_open);
  RUN_TEST(test_symbols_linear_steps_one_cell_and_wraps);
  RUN_TEST(test_emoji_tray_grid_and_commands_row);
  RUN_TEST(test_emoji_tray_spaced_out);
  RUN_TEST(test_input_emoji_button_on_touch);
  RUN_TEST(test_emoji_tray_taps);
  RUN_TEST(test_type_filter);
  RUN_TEST(test_symbol_table_backspace_closes);
  RUN_TEST(test_emoji_tray_render_draws_both_halves);
  RUN_TEST(test_put_text_wide_emoji);
  RUN_TEST(test_input_line_renders_wide_emoji);
  RUN_TEST(test_text_area_wraps_by_width);
  return UNITY_END();
}
