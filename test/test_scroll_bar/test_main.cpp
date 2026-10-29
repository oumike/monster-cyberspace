#include <unity.h>

#include "ui/grid.h"
#include "ui/theme.h"
#include "ui/widgets/scroll_bar.h"

using namespace rc;

void setUp() {}
void tearDown() {}

void test_thumb_ends() {
  ScrollThumb a = scrollThumb(10, 100, 10, 0);
  TEST_ASSERT_EQUAL(0, a.pos);
  TEST_ASSERT_EQUAL(1, a.len);
  ScrollThumb b = scrollThumb(10, 100, 10, 90);
  TEST_ASSERT_EQUAL(9, b.pos);
  ScrollThumb c = scrollThumb(10, 20, 10, 5);
  TEST_ASSERT_EQUAL(5, c.len);
  TEST_ASSERT_TRUE(c.pos > 0 && c.pos < 5);
}

// Only the very top or bottom touches the ends, so "more above" always shows.
void test_thumb_leaves_the_top_once_scrolled() {
  TEST_ASSERT_EQUAL(1, scrollThumb(10, 1000, 10, 1).pos);
  TEST_ASSERT_EQUAL(8, scrollThumb(10, 1000, 10, 989).pos);
}

void test_draws_only_when_it_overflows() {
  Grid g(5, 4);
  const Theme& t = defaultTheme();
  drawScrollBar(g, 4, 0, 4, 4, 4, 0, t, t.bg);
  TEST_ASSERT_EQUAL(U' ', g.at(4, 0).cp);
  drawScrollBar(g, 4, 0, 4, 8, 4, 0, t, t.bg);
  TEST_ASSERT_EQUAL(kScrollBarCp, g.at(4, 0).cp);
  TEST_ASSERT_EQUAL_HEX16(t.fg, g.at(4, 0).fg);
  TEST_ASSERT_EQUAL_HEX16(t.fg, g.at(4, 1).fg);
  TEST_ASSERT_EQUAL_HEX16(t.barBg, g.at(4, 3).fg);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_thumb_ends);
  RUN_TEST(test_thumb_leaves_the_top_once_scrolled);
  RUN_TEST(test_draws_only_when_it_overflows);
  return UNITY_END();
}
