#include <unity.h>

#include <cstring>

#include "ui/brightness.h"
#include "ui/kb_light.h"
#include "ui/screen_timeout.h"

using namespace rc;

void setUp() {}
void tearDown() {}

void test_timeout_options_cycle_and_labels() {
  TEST_ASSERT_EQUAL_UINT32(60, nextScreenTimeout(30));
  TEST_ASSERT_EQUAL_UINT32(0, nextScreenTimeout(3600));  // then Never
  TEST_ASSERT_EQUAL_UINT32(15, nextScreenTimeout(0));    // and round again
  TEST_ASSERT_EQUAL_UINT32(15, nextScreenTimeout(45));   // off the list: the first
  TEST_ASSERT_EQUAL_STRING("15 s", screenTimeoutLabel(15).c_str());
  TEST_ASSERT_EQUAL_STRING("2 min", screenTimeoutLabel(120).c_str());
  TEST_ASSERT_EQUAL_STRING("Never", screenTimeoutLabel(0).c_str());
  TEST_ASSERT_TRUE(validScreenTimeout(1800));
  TEST_ASSERT_TRUE(validScreenTimeout(0));
  TEST_ASSERT_FALSE(validScreenTimeout(45));
}

void test_idle_phases() {
  // 30 s: full until 20 s, dimmed for the last 10 s, then locked.
  TEST_ASSERT_EQUAL(int(IdlePhase::Active), int(idlePhase(19999, 30)));
  TEST_ASSERT_EQUAL(int(IdlePhase::Dim), int(idlePhase(20000, 30)));
  TEST_ASSERT_EQUAL(int(IdlePhase::Lock), int(idlePhase(30000, 30)));
  // 15 s dims after 5 s; Never never moves.
  TEST_ASSERT_EQUAL(int(IdlePhase::Dim), int(idlePhase(5000, 15)));
  TEST_ASSERT_EQUAL(int(IdlePhase::Active), int(idlePhase(int64_t(1) << 40, 0)));
}

void test_bouncer_stays_inside_and_reflects() {
  Bouncer b{0, 2, 1, -1};
  b.step(3, 2);  // a 4 x 3 box of positions
  TEST_ASSERT_EQUAL(1, b.x);
  TEST_ASSERT_EQUAL(1, b.y);
  for (int i = 0; i < 50; ++i) {
    b.step(3, 2);
    TEST_ASSERT_TRUE(b.x >= 0 && b.x <= 3);
    TEST_ASSERT_TRUE(b.y >= 0 && b.y <= 2);
  }
  Bouncer still{0, 0, 1, 1};
  still.step(0, 0);  // no room to move
  TEST_ASSERT_EQUAL(0, still.x);
  TEST_ASSERT_EQUAL(0, still.y);
}

void test_kb_light_levels() {
  using rc::KbLight;
  TEST_ASSERT_EQUAL_UINT8(0, rc::kbLightDuty(KbLight::Off));
  TEST_ASSERT_EQUAL_UINT8(64, rc::kbLightDuty(KbLight::Low));
  TEST_ASSERT_EQUAL_UINT8(160, rc::kbLightDuty(KbLight::Medium));
  TEST_ASSERT_EQUAL_UINT8(255, rc::kbLightDuty(KbLight::High));
  TEST_ASSERT(rc::kbLightFromIndex(2) == KbLight::Medium);
  TEST_ASSERT(rc::kbLightFromIndex(9, KbLight::High) == KbLight::High);
  TEST_ASSERT(rc::kbLightFromIndex(-1) == KbLight::Off);
  KbLight l = KbLight::Off;
  TEST_ASSERT_TRUE(rc::parseKbLight("medium", l));
  TEST_ASSERT(l == KbLight::Medium);
  TEST_ASSERT_FALSE(rc::parseKbLight("dim", l));
  TEST_ASSERT(l == KbLight::Medium);
  TEST_ASSERT_EQUAL_STRING("high", rc::kbLightKey(KbLight::High));
}

// F11's step follows Settings' order and wraps through Off.
void test_kb_light_step_follows_settings_order() {
  using rc::KbLight;
  TEST_ASSERT(rc::nextKbLight(KbLight::Low) == KbLight::Medium);
  TEST_ASSERT(rc::nextKbLight(KbLight::Medium) == KbLight::High);
  TEST_ASSERT(rc::nextKbLight(KbLight::High) == KbLight::Off);
  TEST_ASSERT(rc::nextKbLight(KbLight::Off) == KbLight::Low);
  TEST_ASSERT(rc::nextKbLight(static_cast<KbLight>(9)) == KbLight::Low);
}

void test_brightness_steps_and_backlight() {
  TEST_ASSERT_TRUE(validBrightness(10));
  TEST_ASSERT_TRUE(validBrightness(100));
  TEST_ASSERT_FALSE(validBrightness(0));
  TEST_ASSERT_FALSE(validBrightness(55));
  TEST_ASSERT_EQUAL_UINT8(60, snapBrightness(57));
  TEST_ASSERT_EQUAL_UINT8(10, snapBrightness(3));
  TEST_ASSERT_EQUAL_UINT8(100, snapBrightness(0));  // unset: full
  // Full brightness leaves every level as it was.
  TEST_ASSERT_EQUAL_UINT8(255, backlightFor(255, 100));
  TEST_ASSERT_EQUAL_UINT8(kDimLevel, backlightFor(kDimLevel, 100));
  // Lower, lit scales, and dim and the screensaver never outshine it.
  TEST_ASSERT_EQUAL_UINT8(128, backlightFor(255, 50));
  TEST_ASSERT_EQUAL_UINT8(kDimLevel, backlightFor(kDimLevel, 50));
  TEST_ASSERT_EQUAL_UINT8(26, backlightFor(255, 10));
  TEST_ASSERT_EQUAL_UINT8(26, backlightFor(kDimLevel, 10));
  TEST_ASSERT_EQUAL_UINT8(26, backlightFor(kSaverLevel, 10));
  TEST_ASSERT_EQUAL_UINT8(0, backlightFor(0, 50));  // off stays off
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_timeout_options_cycle_and_labels);
  RUN_TEST(test_idle_phases);
  RUN_TEST(test_bouncer_stays_inside_and_reflects);
  RUN_TEST(test_kb_light_levels);
  RUN_TEST(test_kb_light_step_follows_settings_order);
  RUN_TEST(test_brightness_steps_and_backlight);
  return UNITY_END();
}
