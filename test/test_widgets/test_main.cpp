#include <unity.h>

#include <string>
#include <vector>

#include "ui/grid.h"
#include "ui/qr_cells.h"
#include "ui/theme.h"
#include "ui/widgets/banner.h"
#include "ui/widgets/drawer.h"
#include "ui/widgets/entry_list.h"
#include "ui/widgets/input_line.h"
#include "ui/widgets/list_view.h"
#include "ui/widgets/log_view.h"
#include "ui/widgets/palette.h"
#include "ui/widgets/status_bar.h"
#include "ui/widgets/text_area.h"
#include "util/utf8.h"

using namespace rc;

void setUp() {}
void tearDown() {}

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
  for (int i = 0; i < 10; ++i) items.push_back({utf8ToU32("item" + std::to_string(i)), U"", 1});
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

void test_list_wraps() {
  ListView lv;
  lv.up();  // empty: no-op
  TEST_ASSERT_EQUAL(0, lv.selected());
  std::vector<ListItem> items;
  for (int i = 0; i < 10; ++i) items.push_back({utf8ToU32("item" + std::to_string(i)), U"", 1});
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
  TEST_ASSERT_EQUAL(15, Drawer::width(TextSize::Compact));
  TEST_ASSERT_EQUAL(15, Drawer::width(TextSize::Comfortable));
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

void test_palette_first_is_text_size() {
  const auto& p = paletteEntries();
  TEST_ASSERT_TRUE(p[0].isTextSize);
  TEST_ASSERT_EQUAL(26, p.size());
  TEST_ASSERT_EQUAL_STRING("/me ", u32ToUtf8(p[1].insert).c_str());
  TEST_ASSERT_EQUAL_STRING("/muted ", u32ToUtf8(p.back().insert).c_str());
}

void test_palette_cmail_drops_circ_only() {
  const auto& p = paletteEntries(PaletteKind::Cmail);
  TEST_ASSERT_TRUE(p[0].isTextSize);
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
  RUN_TEST(test_log_preserves_selection_on_setLines);
  RUN_TEST(test_list_scrolls);
  RUN_TEST(test_list_wraps);
  RUN_TEST(test_drawer_wraps);
  RUN_TEST(test_drawer_idle_marker);
  RUN_TEST(test_status_bar_layout);
  RUN_TEST(test_banner_expires);
  RUN_TEST(test_palette_first_is_text_size);
  RUN_TEST(test_palette_cmail_drops_circ_only);
  RUN_TEST(test_qr_small_url);
  RUN_TEST(test_entry_list_outline);
  RUN_TEST(test_entry_list_scrolls);
  RUN_TEST(test_entry_list_wraps);
  RUN_TEST(test_text_area_newlines_and_cap);
  RUN_TEST(test_text_area_up_down);
  RUN_TEST(test_text_area_render_cursor_visible);
  return UNITY_END();
}
