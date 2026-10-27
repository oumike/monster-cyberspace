#include <unity.h>

#include <vector>

#include "input/gestures.h"
#include "input/touch_transform.h"
#include "input/key_event.h"
#include "input/touch_resolve.h"
#include "ui/tap_map.h"
#include "ui/grid.h"
#include "ui/rotation.h"

using namespace rc;

void setUp() {}
void tearDown() {}

struct Sample {
  bool down;
  int16_t x, y;
  uint32_t ms;
};

// Feeds a list of samples and collects every gesture produced.
static std::vector<Gesture> feedAll(Gestures& g, const std::vector<Sample>& ss) {
  std::vector<Gesture> out;
  for (const auto& s : ss) {
    Gesture gg;
    if (g.feed(s.down, s.x, s.y, s.ms, gg)) out.push_back(gg);
  }
  return out;
}

void test_tap() {
  Gestures g;
  auto gs = feedAll(g, {{true, 100, 100, 0}, {true, 104, 103, 50}, {false, 104, 103, 200}});
  TEST_ASSERT_EQUAL(1u, gs.size());
  TEST_ASSERT_EQUAL(int(GestureKind::Tap), int(gs[0].kind));
  TEST_ASSERT_EQUAL(100, gs[0].x);
  TEST_ASSERT_EQUAL(100, gs[0].y);
}

void test_tap_late_is_not_tap() {
  Gestures g;
  auto gs = feedAll(g, {{true, 100, 100, 0}, {true, 100, 100, 500}, {false, 100, 100, 520}});
  // The @500 sample fires a LongPress; the late up is not a Tap.
  TEST_ASSERT_EQUAL(1u, gs.size());
  TEST_ASSERT_EQUAL(int(GestureKind::LongPress), int(gs[0].kind));
}

void test_movement_cancels_tap() {
  Gestures g;
  auto gs = feedAll(g, {{true, 100, 100, 0}, {true, 111, 100, 50}, {false, 111, 100, 100}});
  TEST_ASSERT_EQUAL(0u, gs.size());
}

void test_long_press_once() {
  Gestures g;
  auto gs = feedAll(g, {{true, 100, 100, 0},
                        {true, 100, 100, 499},
                        {true, 100, 100, 500},
                        {true, 100, 100, 700},
                        {false, 100, 100, 800}});
  TEST_ASSERT_EQUAL(1u, gs.size());
  TEST_ASSERT_EQUAL(int(GestureKind::LongPress), int(gs[0].kind));
  TEST_ASSERT_EQUAL(100, gs[0].x);
  TEST_ASSERT_EQUAL(100, gs[0].y);
}

void test_movement_cancels_long_press() {
  Gestures g;
  auto gs = feedAll(g, {{true, 100, 100, 0}, {true, 111, 100, 300}, {true, 111, 100, 900}});
  TEST_ASSERT_EQUAL(0u, gs.size());
}

void test_drag_vertical() {
  Gestures g;
  auto gs = feedAll(g, {{true, 100, 100, 0},
                        {true, 100, 105, 20},
                        {true, 100, 111, 40},
                        {true, 100, 115, 60},
                        {true, 100, 110, 80}});
  TEST_ASSERT_EQUAL(3u, gs.size());
  TEST_ASSERT_EQUAL(int(GestureKind::Drag), int(gs[0].kind));
  TEST_ASSERT_EQUAL(11, gs[0].dy);
  TEST_ASSERT_EQUAL(4, gs[1].dy);
  TEST_ASSERT_EQUAL(-5, gs[2].dy);
}

void test_swipe_four_directions() {
  {
    Gestures g;
    auto gs = feedAll(g, {{true, 100, 100, 0}, {false, 100, 59, 299}});  // up 41
    TEST_ASSERT_EQUAL(1u, gs.size());
    TEST_ASSERT_EQUAL(int(GestureKind::Swipe), int(gs[0].kind));
    TEST_ASSERT_EQUAL(int(SwipeDir::Up), int(gs[0].dir));
  }
  {
    Gestures g;
    auto gs = feedAll(g, {{true, 100, 100, 0}, {false, 100, 141, 299}});  // down 41
    TEST_ASSERT_EQUAL(1u, gs.size());
    TEST_ASSERT_EQUAL(int(SwipeDir::Down), int(gs[0].dir));
  }
  {
    Gestures g;
    auto gs = feedAll(g, {{true, 100, 100, 0}, {false, 59, 100, 299}});  // left 41
    TEST_ASSERT_EQUAL(1u, gs.size());
    TEST_ASSERT_EQUAL(int(SwipeDir::Left), int(gs[0].dir));
  }
  {
    Gestures g;
    auto gs = feedAll(g, {{true, 100, 100, 0}, {false, 141, 100, 299}});  // right 41
    TEST_ASSERT_EQUAL(1u, gs.size());
    TEST_ASSERT_EQUAL(int(SwipeDir::Right), int(gs[0].dir));
  }
}

void test_swipe_boundaries() {
  {
    Gestures g;
    auto gs = feedAll(g, {{true, 100, 100, 0}, {false, 140, 100, 200}});  // 40 px
    TEST_ASSERT_EQUAL(0u, gs.size());
  }
  {
    Gestures g;
    auto gs = feedAll(g, {{true, 100, 100, 0}, {false, 141, 100, 300}});  // 41 px @300
    TEST_ASSERT_EQUAL(0u, gs.size());
  }
}

void test_swipe_dominant_axis() {
  Gestures g;
  auto gs = feedAll(g, {{true, 100, 100, 0}, {false, 145, 70, 200}});  // dx+45 dy-30
  TEST_ASSERT_EQUAL(1u, gs.size());
  TEST_ASSERT_EQUAL(int(GestureKind::Swipe), int(gs[0].kind));
  TEST_ASSERT_EQUAL(int(SwipeDir::Right), int(gs[0].dir));
}

void test_edge_swipe_back() {
  {
    Gestures g;
    auto gs = feedAll(g, {{true, 15, 100, 0}, {false, 60, 100, 200}});
    TEST_ASSERT_EQUAL(1u, gs.size());
    TEST_ASSERT_EQUAL(int(GestureKind::EdgeBack), int(gs[0].kind));
  }
  {
    Gestures g;
    auto gs = feedAll(g, {{true, 16, 100, 0}, {false, 61, 100, 200}});
    TEST_ASSERT_EQUAL(1u, gs.size());
    TEST_ASSERT_EQUAL(int(GestureKind::Swipe), int(gs[0].kind));
    TEST_ASSERT_EQUAL(int(SwipeDir::Right), int(gs[0].dir));
  }
}

void test_reset_ignores_finger_already_down() {
  Gestures g;
  Gesture gg;
  TEST_ASSERT_FALSE(g.feed(true, 100, 100, 0, gg));
  g.reset();
  for (uint32_t t = 100; t <= 900; t += 100) {
    TEST_ASSERT_FALSE(g.feed(true, 100, 100, t, gg));
  }
  TEST_ASSERT_FALSE(g.feed(false, 100, 100, 1000, gg));
  // A fresh touch afterwards taps normally.
  auto gs = feedAll(g, {{true, 50, 50, 1100}, {false, 50, 50, 1200}});
  TEST_ASSERT_EQUAL(1u, gs.size());
  TEST_ASSERT_EQUAL(int(GestureKind::Tap), int(gs[0].kind));
}

void test_transform_landscape_matches_bringup() {
  int16_t x, y;
  touchToFrame(0, 0, 240, 320, 0, x, y);
  TEST_ASSERT_EQUAL(319, x);
  TEST_ASSERT_EQUAL(0, y);
  touchToFrame(239, 319, 240, 320, 0, x, y);
  TEST_ASSERT_EQUAL(0, x);
  TEST_ASSERT_EQUAL(239, y);
}

void test_transform_rotations_corners() {
  const int W = 240, H = 320;
  const int16_t raws[4][2] = {{0, 0}, {239, 0}, {239, 319}, {0, 319}};
  for (uint8_t r = 0; r < 4; ++r) {
    int fw = (r % 2 == 0) ? H : W;
    int fh = (r % 2 == 0) ? W : H;
    bool seen[4] = {false, false, false, false};
    for (auto& rp : raws) {
      int16_t x, y;
      touchToFrame(rp[0], rp[1], W, H, r, x, y);
      bool xEdge = (x == 0) || (x == fw - 1);
      bool yEdge = (y == 0) || (y == fh - 1);
      TEST_ASSERT_TRUE(xEdge && yEdge);
      int idx = (x == 0 ? 0 : 1) + (y == 0 ? 0 : 2);
      TEST_ASSERT_FALSE(seen[idx]);
      seen[idx] = true;
    }
    TEST_ASSERT_TRUE(seen[0] && seen[1] && seen[2] && seen[3]);
  }
  // r2 equals (319-x0, 239-y0) of r0.
  int16_t x0, y0, x2, y2;
  touchToFrame(130, 200, W, H, 0, x0, y0);
  touchToFrame(130, 200, W, H, 2, x2, y2);
  TEST_ASSERT_EQUAL(319 - x0, x2);
  TEST_ASSERT_EQUAL(239 - y0, y2);
}

void test_transform_clamps() {
  int16_t x, y;
  touchToFrame(-5, 400, 240, 320, 0, x, y);
  TEST_ASSERT_EQUAL(0, x);
  TEST_ASSERT_EQUAL(0, y);
  touchToFrame(300, -1, 240, 320, 1, x, y);
  TEST_ASSERT_TRUE(x >= 0 && x <= 239);
  TEST_ASSERT_TRUE(y >= 0 && y <= 319);
}

// ---- Task 2: key events, the tap map and the touch resolver ----

static KeyEvent ev(Action a) {
  KeyEvent e;
  e.action = a;
  return e;
}

static Target rowTarget(ListId l, int idx) {
  Target t;
  t.kind = TargetKind::Row;
  t.list = l;
  t.index = (int16_t)idx;
  return t;
}

static Target fixedTarget(KeyEvent tap, KeyEvent hold = KeyEvent{}) {
  Target t;
  t.kind = TargetKind::Fixed;
  t.tap = tap;
  t.hold = hold;
  return t;
}

static Gesture mkG(GestureKind k, int16_t x, int16_t y) {
  Gesture g;
  g.kind = k;
  g.x = x;
  g.y = y;
  return g;
}

void test_tap_map_cells_to_px() {
  TapMap m;
  m.setOrigin(CellSize{6, 12}, 10);
  m.addCells(2, 0, 4, 0, rowTarget(ListId::Main, 3));
  Target h = m.hit(12, 120);
  TEST_ASSERT_EQUAL(int(TargetKind::Row), int(h.kind));
  TEST_ASSERT_EQUAL(3, h.index);
  TEST_ASSERT_EQUAL(int(TargetKind::None), int(m.hit(11, 120).kind));
  TEST_ASSERT_EQUAL(int(TargetKind::None), int(m.hit(12, 119).kind));
  TEST_ASSERT_EQUAL(int(TargetKind::None), int(m.hit(30, 120).kind));
}

void test_tap_map_topmost_wins() {
  TapMap m;
  m.setOrigin(CellSize{6, 12}, 0);
  m.addCells(0, 0, 10, 0, rowTarget(ListId::Main, 1));
  m.addCells(0, 0, 10, 0, fixedTarget(ev(Action::Back)));
  Target h = m.hit(6, 6);
  TEST_ASSERT_EQUAL(int(TargetKind::Fixed), int(h.kind));
  TEST_ASSERT_EQUAL(int(Action::Back), int(h.tap.action));
}

void test_tap_map_rows_follow_spans() {
  // A Feed-like grid: row 0 bar, rows 1/3 Big entry heads (h 16), rows 2/4
  // bodies, row 5 Skip, row 6 the hint. Each entry is two rows.
  Grid g(20, 7);
  g.setRowKind(1, RowKind::Big);
  g.setRowKind(3, RowKind::Big);
  g.setRowKind(5, RowKind::Skip);
  TapMap m;
  m.setOrigin(CellSize{6, 12}, 0);
  m.addCells(0, 1, 19, 2, rowTarget(ListId::Main, 0));
  m.addCells(0, 3, 19, 4, rowTarget(ListId::Main, 1));
  m.addCells(0, 6, 19, 6, fixedTarget(ev(Action::Back)));
  m.mapRows(0, rowSpans(g, 12, 16));
  // Spans: 0 [0,12) 1 [12,28) 2 [28,40) 3 [40,56) 4 [56,68) 5 empty 6 [72,84).
  TEST_ASSERT_EQUAL(0, m.hit(5, 39).index);
  TEST_ASSERT_EQUAL(1, m.hit(5, 40).index);  // uniform rows would still say entry 0
  TEST_ASSERT_EQUAL(1, m.hit(5, 67).index);
  TEST_ASSERT_EQUAL(int(TargetKind::None), int(m.hit(5, 68).kind));
  TEST_ASSERT_EQUAL(int(TargetKind::Fixed), int(m.hit(5, 72).kind));
}

void test_tap_map_rows_leave_other_grids() {
  // The keyboard grid below the screen keeps its uniform rows.
  Grid g(20, 3);
  g.setRowKind(0, RowKind::Big);
  TapMap m;
  m.setOrigin(CellSize{6, 12}, 3);
  m.addCells(0, 0, 19, 0, rowTarget(ListId::Main, 7));
  m.mapRows(0, rowSpans(g, 12, 16));
  TEST_ASSERT_EQUAL(7, m.hit(5, 36).index);
  TEST_ASSERT_EQUAL(int(TargetKind::None), int(m.hit(5, 35).kind));
}

void test_tap_map_clear() {
  TapMap m;
  m.setOrigin(CellSize{6, 12}, 0);
  m.addCells(0, 0, 4, 0, rowTarget(ListId::Main, 0));
  m.setList(ListId::Main, 0, 3, true);
  TEST_ASSERT_TRUE(m.size() > 0);
  m.clear();
  TEST_ASSERT_EQUAL(0u, m.size());
  TEST_ASSERT_NULL(m.list(ListId::Main));
}

void test_field_hit_position() {
  TapMap m;
  m.setOrigin(CellSize{6, 12}, 0);
  Target f;
  f.kind = TargetKind::Field;
  f.field = 2;
  f.first = 5;
  f.len = 3;
  m.addCells(2, 0, 10, 0, f);
  Target h = m.hit(4 * 6, 6);  // col 4 → first + 2 = 7
  TEST_ASSERT_EQUAL(int(TargetKind::Field), int(h.kind));
  TEST_ASSERT_EQUAL(7, h.index);
  Target h2 = m.hit(9 * 6, 6);  // col 9 → capped at first+len = 8
  TEST_ASSERT_EQUAL(8, h2.index);
}

void test_resolve_row_tap_and_hold() {
  TapMap m;
  m.setOrigin(CellSize{6, 12}, 0);
  m.addCells(0, 0, 10, 0, rowTarget(ListId::Main, 3));
  TouchResolver r;
  TouchOutput out;
  r.resolve(mkG(GestureKind::Tap, 6, 6), m, out);
  TEST_ASSERT_EQUAL(1u, out.keys.size());
  TEST_ASSERT_EQUAL(int(Action::Activate), int(out.keys[0].action));
  TEST_ASSERT_EQUAL(3, out.keys[0].index);
  TEST_ASSERT_EQUAL(int(ListId::Main), out.keys[0].target);
  TEST_ASSERT_EQUAL(int(Action::Select), int(out.keys[0].then));
  TouchOutput out2;
  r.resolve(mkG(GestureKind::LongPress, 6, 6), m, out2);
  TEST_ASSERT_EQUAL(1u, out2.keys.size());
  TEST_ASSERT_EQUAL(int(Action::Palette), int(out2.keys[0].then));
}

void test_resolve_fixed() {
  TapMap m;
  m.setOrigin(CellSize{6, 12}, 0);
  m.addCells(0, 0, 10, 0, fixedTarget(ev(Action::Back), ev(Action::OpenHome)));
  TouchResolver r;
  TouchOutput out;
  r.resolve(mkG(GestureKind::Tap, 6, 6), m, out);
  TEST_ASSERT_EQUAL(int(Action::Back), int(out.keys[0].action));
  TouchOutput out2;
  r.resolve(mkG(GestureKind::LongPress, 6, 6), m, out2);
  TEST_ASSERT_EQUAL(int(Action::OpenHome), int(out2.keys[0].action));

  TapMap m2;
  m2.setOrigin(CellSize{6, 12}, 0);
  m2.addCells(0, 0, 10, 0, fixedTarget(ev(Action::Palette)));
  TouchOutput out3;
  r.resolve(mkG(GestureKind::LongPress, 6, 6), m2, out3);
  TEST_ASSERT_EQUAL(int(Action::Palette), int(out3.keys[0].action));
}

void test_resolve_field_focus() {
  TapMap m;
  m.setOrigin(CellSize{6, 12}, 0);
  Target f;
  f.kind = TargetKind::Field;
  f.field = 2;
  f.first = 5;
  f.len = 3;
  m.addCells(2, 0, 10, 0, f);
  TouchResolver r;
  TouchOutput out;
  r.resolve(mkG(GestureKind::Tap, 4 * 6, 6), m, out);
  TEST_ASSERT_EQUAL(1u, out.keys.size());
  TEST_ASSERT_EQUAL(int(Action::Focus), int(out.keys[0].action));
  TEST_ASSERT_EQUAL(2, out.keys[0].target);
  TEST_ASSERT_EQUAL(7, out.keys[0].index);
}

void test_resolve_osk_hold_is_tap() {
  TapMap m;
  m.setOrigin(CellSize{6, 12}, 0);
  Target o;
  o.kind = TargetKind::Osk;
  o.index = 12;
  m.addCells(0, 0, 10, 0, o);
  TouchResolver r;
  TouchOutput out;
  r.resolve(mkG(GestureKind::Tap, 6, 6), m, out);
  TEST_ASSERT_EQUAL(12, out.oskKey);
  TEST_ASSERT_EQUAL(0u, out.keys.size());
  TouchOutput out2;
  r.resolve(mkG(GestureKind::LongPress, 6, 6), m, out2);
  TEST_ASSERT_EQUAL(12, out2.oskKey);
  TEST_ASSERT_EQUAL(0u, out2.keys.size());
}

void test_resolve_banner() {
  TapMap m;
  m.setOrigin(CellSize{6, 12}, 0);
  Target b;
  b.kind = TargetKind::Banner;
  m.addCells(0, 0, 10, 0, b);
  TouchResolver r;
  TouchOutput out;
  r.resolve(mkG(GestureKind::Tap, 6, 6), m, out);
  TEST_ASSERT_TRUE(out.hideBanner);
  TEST_ASSERT_EQUAL(0u, out.keys.size());
}

void test_resolve_drag_log_steps() {
  TapMap m;
  m.setOrigin(CellSize{6, 12}, 0);
  m.addCells(0, 0, 10, 0, rowTarget(ListId::Log, 0));
  m.setList(ListId::Log, 0, 100, false);
  TouchResolver r;
  {
    TouchOutput out;
    Gesture g = mkG(GestureKind::Drag, 6, 6);
    g.dy = 13;
    r.resolve(g, m, out);
    TEST_ASSERT_EQUAL(1u, out.keys.size());
    TEST_ASSERT_EQUAL(int(Action::Up), int(out.keys[0].action));
  }
  {
    TouchOutput out;
    Gesture g = mkG(GestureKind::Drag, 6, 6);
    g.dy = 11;
    r.resolve(g, m, out);
    TEST_ASSERT_EQUAL(1u, out.keys.size());
    TEST_ASSERT_EQUAL(int(Action::Up), int(out.keys[0].action));
  }
  {
    TouchOutput out;
    Gesture g = mkG(GestureKind::Drag, 6, 6);
    g.dy = -24;
    r.resolve(g, m, out);
    TEST_ASSERT_EQUAL(2u, out.keys.size());
    TEST_ASSERT_EQUAL(int(Action::Down), int(out.keys[0].action));
    TEST_ASSERT_EQUAL(int(Action::Down), int(out.keys[1].action));
  }
}

void test_resolve_drag_wrapping_list_clamps() {
  {
    TapMap m;
    m.setOrigin(CellSize{6, 12}, 0);
    m.addCells(0, 0, 10, 0, rowTarget(ListId::Main, 1));
    m.setList(ListId::Main, 1, 5, true);
    TouchResolver r;
    TouchOutput out;
    Gesture g = mkG(GestureKind::Drag, 6, 6);
    g.dy = -72;  // 6 steps later
    r.resolve(g, m, out);
    TEST_ASSERT_EQUAL(1u, out.keys.size());
    TEST_ASSERT_EQUAL(int(Action::Activate), int(out.keys[0].action));
    TEST_ASSERT_EQUAL(4, out.keys[0].index);
    TEST_ASSERT_EQUAL(int(Action::None), int(out.keys[0].then));
  }
  {
    TapMap m;
    m.setOrigin(CellSize{6, 12}, 0);
    m.addCells(0, 0, 10, 0, rowTarget(ListId::Main, 0));
    m.setList(ListId::Main, 0, 5, true);
    TouchResolver r;
    TouchOutput out;
    Gesture g = mkG(GestureKind::Drag, 6, 6);
    g.dy = 36;  // 3 steps earlier, clamps at 0 → nothing
    r.resolve(g, m, out);
    TEST_ASSERT_EQUAL(0u, out.keys.size());
  }
}

void test_resolve_swipe_pages() {
  TapMap m;
  m.setOrigin(CellSize{6, 12}, 0);
  for (int i = 0; i < 6; ++i) m.addCells(0, i, 10, i, rowTarget(ListId::Main, i));
  m.setList(ListId::Main, 0, 20, true);
  TouchResolver r;
  TouchOutput out;
  Gesture g = mkG(GestureKind::Swipe, 6, 6);
  g.dir = SwipeDir::Up;
  r.resolve(g, m, out);
  TEST_ASSERT_EQUAL(1u, out.keys.size());
  TEST_ASSERT_EQUAL(int(Action::Activate), int(out.keys[0].action));
  TEST_ASSERT_EQUAL(5, out.keys[0].index);
}

void test_resolve_edge_back() {
  TapMap m;
  m.setOrigin(CellSize{6, 12}, 0);
  TouchResolver r;
  TouchOutput out;
  r.resolve(mkG(GestureKind::EdgeBack, 2, 6), m, out);
  TEST_ASSERT_EQUAL(1u, out.keys.size());
  TEST_ASSERT_EQUAL(int(Action::Back), int(out.keys[0].action));
}

void test_resolve_nothing_hit() {
  TapMap m;
  m.setOrigin(CellSize{6, 12}, 0);
  TouchResolver r;
  TouchOutput out;
  r.resolve(mkG(GestureKind::Tap, 6, 6), m, out);
  TEST_ASSERT_EQUAL(0u, out.keys.size());
  TEST_ASSERT_EQUAL(-1, out.oskKey);
  TEST_ASSERT_FALSE(out.hideBanner);
}

void test_power_tap_peeks_not_wakes() {
  KeyEvent e;
  e.action = Action::PowerTap;
  TEST_ASSERT_TRUE(peeksScreen(e));
  TEST_ASSERT_FALSE(wakesScreen(e, false));
}

void test_rotation_labels_and_order() {
  const char* want[4] = {"Landscape", "Landscape 180", "Portrait", "Portrait 180"};
  for (int i = 0; i < 4; ++i) TEST_ASSERT_EQUAL_STRING(want[i], rotationLabel(kRotationOrder[i]));
  TEST_ASSERT_FALSE(rotationPortrait(0));
  TEST_ASSERT_TRUE(rotationPortrait(1));
  TEST_ASSERT_FALSE(rotationPortrait(2));
  TEST_ASSERT_TRUE(rotationPortrait(3));
}

void test_rotation_from_index_out_of_range() {
  TEST_ASSERT_EQUAL(0, rotationFromIndex(-1));
  TEST_ASSERT_EQUAL(0, rotationFromIndex(4));
  TEST_ASSERT_EQUAL(0, rotationFromIndex(255));
  TEST_ASSERT_EQUAL(3, rotationFromIndex(3));
  TEST_ASSERT_EQUAL(1, rotationFromIndex(1));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_tap);
  RUN_TEST(test_tap_late_is_not_tap);
  RUN_TEST(test_movement_cancels_tap);
  RUN_TEST(test_long_press_once);
  RUN_TEST(test_movement_cancels_long_press);
  RUN_TEST(test_drag_vertical);
  RUN_TEST(test_swipe_four_directions);
  RUN_TEST(test_swipe_boundaries);
  RUN_TEST(test_swipe_dominant_axis);
  RUN_TEST(test_edge_swipe_back);
  RUN_TEST(test_reset_ignores_finger_already_down);
  RUN_TEST(test_transform_landscape_matches_bringup);
  RUN_TEST(test_transform_rotations_corners);
  RUN_TEST(test_transform_clamps);
  RUN_TEST(test_tap_map_cells_to_px);
  RUN_TEST(test_tap_map_topmost_wins);
  RUN_TEST(test_tap_map_clear);
  RUN_TEST(test_tap_map_rows_follow_spans);
  RUN_TEST(test_tap_map_rows_leave_other_grids);
  RUN_TEST(test_field_hit_position);
  RUN_TEST(test_resolve_row_tap_and_hold);
  RUN_TEST(test_resolve_fixed);
  RUN_TEST(test_resolve_field_focus);
  RUN_TEST(test_resolve_osk_hold_is_tap);
  RUN_TEST(test_resolve_banner);
  RUN_TEST(test_resolve_drag_log_steps);
  RUN_TEST(test_resolve_drag_wrapping_list_clamps);
  RUN_TEST(test_resolve_swipe_pages);
  RUN_TEST(test_resolve_edge_back);
  RUN_TEST(test_resolve_nothing_hit);
  RUN_TEST(test_power_tap_peeks_not_wakes);
  RUN_TEST(test_rotation_labels_and_order);
  RUN_TEST(test_rotation_from_index_out_of_range);
  return UNITY_END();
}
