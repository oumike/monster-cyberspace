#include <unity.h>

#include <string>

#include "input/key_event.h"
#include "ui/grid.h"
#include "ui/tap_map.h"
#include "ui/theme.h"
#include "ui/widgets/scroll_bar.h"
#include "ui/widgets/text_view.h"
#include "util/utf8.h"

using namespace rc;

void setUp() {}
void tearDown() { setActiveTaps(nullptr); }

static std::string rowText(const Grid& g, int row, int from = 0, int to = -1) {
  if (to < 0) to = g.cols();
  std::u32string s;
  for (int c = from; c < to; ++c) s += g.at(c, row).cp;
  while (!s.empty() && s.back() == U' ') s.pop_back();
  return u32ToUtf8(s);
}

void test_headings_lose_their_marks_and_are_bold() {
  Grid g(20, 4);
  TextView v;
  v.setText(U"### Changes\nplain");
  v.render(g, 0, 4, 0, 20, defaultTheme());
  TEST_ASSERT_EQUAL_STRING("Changes", rowText(g, 0).c_str());
  TEST_ASSERT_TRUE(g.at(0, 0).bold);
  TEST_ASSERT_EQUAL_STRING("plain", rowText(g, 1).c_str());
  TEST_ASSERT_FALSE(g.at(0, 1).bold);
}

void test_bullets_hang() {
  Grid g(12, 4);
  TextView v;
  v.setText(U"- one two three four");
  v.render(g, 0, 4, 0, 12, defaultTheme());
  TEST_ASSERT_EQUAL_STRING("- one two", rowText(g, 0).c_str());
  TEST_ASSERT_EQUAL_STRING("  three four", rowText(g, 1).c_str());
}

void test_scrolls_and_clamps() {
  Grid g(10, 3);
  TextView v;
  v.setText(U"a\nb\nc\nd\ne");
  v.render(g, 0, 3, 0, 10, defaultTheme());
  TEST_ASSERT_EQUAL_STRING("a", rowText(g, 0, 0, 9).c_str());
  v.up();
  TEST_ASSERT_EQUAL(0, v.top());
  v.down();
  v.down();
  v.down();  // only two lines are hidden
  TEST_ASSERT_EQUAL(2, v.top());
  v.render(g, 0, 3, 0, 10, defaultTheme());
  TEST_ASSERT_EQUAL_STRING("c", rowText(g, 0, 0, 9).c_str());
  TEST_ASSERT_EQUAL_STRING("e", rowText(g, 2, 0, 9).c_str());
  v.pageUp();
  TEST_ASSERT_EQUAL(0, v.top());
  v.pageDown();
  TEST_ASSERT_EQUAL(2, v.top());
  v.setText(U"x");
  TEST_ASSERT_EQUAL(0, v.top());
}

void test_scrollbar_only_when_it_overflows() {
  Grid g(10, 3);
  TextView v;
  v.setText(U"a\nb");
  v.render(g, 0, 3, 0, 10, defaultTheme());
  TEST_ASSERT_EQUAL(U' ', g.at(9, 0).cp);
  v.setText(U"a\nb\nc\nd\ne\nf");
  v.render(g, 0, 3, 0, 10, defaultTheme());
  TEST_ASSERT_EQUAL(kScrollBarCp, g.at(9, 0).cp);
  TEST_ASSERT_EQUAL_HEX16(defaultTheme().fg, g.at(9, 0).fg);  // thumb at the top
  TEST_ASSERT_EQUAL_HEX16(defaultTheme().barBg, g.at(9, 2).fg);
}

void test_rows_register_for_drag_scrolling() {
  TapMap m;
  m.setOrigin(CellSize{8, 16}, 0);
  setActiveTaps(&m);
  Grid g(10, 3);
  TextView v;
  v.setText(U"a\nb\nc\nd");
  v.render(g, 0, 3, 0, 10, defaultTheme());
  const ListInfo* li = m.list(ListId::Log);
  TEST_ASSERT_NOT_NULL(li);
  TEST_ASSERT_FALSE(li->wraps);
  TEST_ASSERT_TRUE(li->holdToAct);  // taps do nothing
  TEST_ASSERT_EQUAL(3, m.visibleRows(ListId::Log));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_headings_lose_their_marks_and_are_bold);
  RUN_TEST(test_bullets_hang);
  RUN_TEST(test_scrolls_and_clamps);
  RUN_TEST(test_scrollbar_only_when_it_overflows);
  RUN_TEST(test_rows_register_for_drag_scrolling);
  return UNITY_END();
}
