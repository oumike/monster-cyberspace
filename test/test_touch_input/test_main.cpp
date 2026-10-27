#include <unity.h>

#include <vector>

#include "input/gestures.h"
#include "input/touch_transform.h"

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
  return UNITY_END();
}
