#include <unity.h>

#include <string>

#include "input/key_event.h"
#include "ui/grid.h"
#include "ui/tap_map.h"
#include "ui/theme.h"
#include "ui/widgets/osk.h"

using namespace rc;

void setUp() {}
void tearDown() {}

static int findChar(const Osk& k, char32_t c) {
  const auto& keys = k.keys();
  for (int i = 0; i < int(keys.size()); ++i)
    if (keys[i].kind == OskKind::Char && keys[i].ch == c) return i;
  return -1;
}

static int findKind(const Osk& k, OskKind kind) {
  const auto& keys = k.keys();
  for (int i = 0; i < int(keys.size()); ++i)
    if (keys[i].kind == kind) return i;
  return -1;
}

static int findLabel(const Osk& k, const std::u32string& label) {
  const auto& keys = k.keys();
  for (int i = 0; i < int(keys.size()); ++i)
    if (keys[i].label == label) return i;
  return -1;
}

void test_osk_rows() {
  TEST_ASSERT_EQUAL(8, Osk::rows(TextSize::Comfortable));
  TEST_ASSERT_EQUAL(12, Osk::rows(TextSize::Compact));
}

void test_osk_layout_landscape_compact() {
  Osk k;
  k.layout(53, TextSize::Compact);
  int q = findChar(k, U'q');
  TEST_ASSERT_TRUE(q >= 0);
  TEST_ASSERT_EQUAL(1, k.keys()[q].col);  // margin (53-50)/2
  TEST_ASSERT_EQUAL(5, k.keys()[q].w);
  TEST_ASSERT_EQUAL(3, k.keys()[q].h);
  TEST_ASSERT_EQUAL(0, k.keys()[q].row);
  int a = findChar(k, U'a');
  TEST_ASSERT_EQUAL(1 + 2, k.keys()[a].col);  // shifted right half a unit
  TEST_ASSERT_EQUAL(3, k.keys()[a].row);
  int space = findKind(k, OskKind::Space);
  TEST_ASSERT_EQUAL(20, k.keys()[space].w);
  const auto& keys = k.keys();
  const OskKey& last = keys.back();
  TEST_ASSERT_EQUAL(int(OskKind::Hide), int(last.kind));
  TEST_ASSERT_EQUAL(9, last.row);
}

void test_osk_layout_portrait() {
  Osk k;
  k.layout(40, TextSize::Compact);
  TEST_ASSERT_EQUAL(4, k.keys()[findChar(k, U'q')].w);
  Osk k2;
  k2.layout(30, TextSize::Comfortable);
  TEST_ASSERT_EQUAL(3, k2.keys()[findChar(k2, U'q')].w);
  TEST_ASSERT_EQUAL(2, k2.keys()[findChar(k2, U'q')].h);
}

void test_osk_page_switching() {
  Osk k;
  k.layout(40, TextSize::Compact);
  k.press(findLabel(k, U"?123"), 0);
  TEST_ASSERT_EQUAL(int(OskPage::Symbols), int(k.page()));
  TEST_ASSERT_EQUAL(int(OskKind::Char), int(k.keys()[0].kind));
  TEST_ASSERT_EQUAL(U'1', k.keys()[0].ch);
  k.press(findLabel(k, U"#+="), 0);
  TEST_ASSERT_EQUAL(int(OskPage::More), int(k.page()));
  k.press(findLabel(k, U"ABC"), 0);
  TEST_ASSERT_EQUAL(int(OskPage::Letters), int(k.page()));
}

void test_osk_shift_one_shot_and_lock() {
  Osk k;
  k.layout(40, TextSize::Compact);
  k.press(findKind(k, OskKind::Shift), 0);
  TEST_ASSERT_TRUE(k.shifted());
  OskOutput a = k.press(findChar(k, U'a'), 10);
  TEST_ASSERT_EQUAL(U'A', a.key.ch);
  OskOutput a2 = k.press(findChar(k, U'a'), 20);
  TEST_ASSERT_EQUAL(U'a', a2.key.ch);
  // second shift tap within 400 ms locks caps
  k.press(findKind(k, OskKind::Shift), 1000);
  k.press(findKind(k, OskKind::Shift), 1200);
  TEST_ASSERT_TRUE(k.capsLock());
  TEST_ASSERT_EQUAL(U'A', k.press(findChar(k, U'a'), 1300).key.ch);
  TEST_ASSERT_EQUAL(U'B', k.press(findChar(k, U'b'), 1400).key.ch);
  TEST_ASSERT_TRUE(k.capsLock());
  // another shift tap turns it off
  k.press(findKind(k, OskKind::Shift), 1500);
  TEST_ASSERT_FALSE(k.capsLock());
  TEST_ASSERT_FALSE(k.shifted());
}

void test_osk_key_events() {
  Osk k;
  k.layout(40, TextSize::Compact);
  OskOutput sp = k.press(findKind(k, OskKind::Space), 0);
  TEST_ASSERT_EQUAL(int(Action::Char), int(sp.key.action));
  TEST_ASSERT_EQUAL(U' ', sp.key.ch);
  OskOutput bs = k.press(findKind(k, OskKind::Backspace), 0);
  TEST_ASSERT_EQUAL(int(Action::Backspace), int(bs.key.action));
  OskOutput en = k.press(findKind(k, OskKind::Enter), 0);
  TEST_ASSERT_EQUAL(int(Action::Select), int(en.key.action));
  OskOutput hd = k.press(findKind(k, OskKind::Hide), 0);
  TEST_ASSERT_TRUE(hd.hide);
}

void test_osk_covers_printable_ascii() {
  bool seen[0x7F] = {false};
  TextSize s = TextSize::Compact;
  OskPage pages[] = {OskPage::Letters, OskPage::Symbols, OskPage::More};
  for (OskPage p : pages) {
    Osk k;
    k.reset();
    k.layout(40, s);
    while (k.page() != p) {
      int next = -1;
      if (k.page() == OskPage::Letters)
        next = findLabel(k, U"?123");
      else if (k.page() == OskPage::Symbols)
        next = findLabel(k, U"#+=");
      k.press(next, 0);
      k.layout(40, s);
    }
    for (int i = 0; i < int(k.keys().size()); ++i) {
      if (k.keys()[i].kind != OskKind::Char) continue;
      char32_t base = k.keys()[i].ch;
      if (base >= 0x21 && base < 0x7F) seen[base] = true;
      if (base >= U'a' && base <= U'z') seen[base - 32] = true;  // shifted
    }
  }
  for (char32_t c = 0x21; c <= 0x7E; ++c)
    TEST_ASSERT_TRUE_MESSAGE(seen[c], std::string(1, char(c)).c_str());
}

void test_osk_registers_targets() {
  Osk k;
  k.layout(53, TextSize::Compact);
  Grid g(53, Osk::rows(TextSize::Compact));
  TapMap m;
  m.setOrigin(cellSize(TextSize::Compact), 0);
  setActiveTaps(&m);
  k.render(g, defaultTheme());
  setActiveTaps(nullptr);
  int q = findChar(k, U'q');
  const OskKey& key = k.keys()[q];
  CellSize cs = cellSize(TextSize::Compact);
  int px = key.col * cs.w + cs.w / 2;
  int py = key.row * cs.h + cs.h / 2;
  Target hit = m.hit(px, py);
  TEST_ASSERT_EQUAL(int(TargetKind::Osk), int(hit.kind));
  TEST_ASSERT_EQUAL(q, hit.index);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_osk_rows);
  RUN_TEST(test_osk_layout_landscape_compact);
  RUN_TEST(test_osk_layout_portrait);
  RUN_TEST(test_osk_page_switching);
  RUN_TEST(test_osk_shift_one_shot_and_lock);
  RUN_TEST(test_osk_key_events);
  RUN_TEST(test_osk_covers_printable_ascii);
  RUN_TEST(test_osk_registers_targets);
  return UNITY_END();
}
