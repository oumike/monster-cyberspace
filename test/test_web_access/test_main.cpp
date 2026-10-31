#include <unity.h>

#include "web/web_access.h"

using namespace rc;

void setUp() {}
void tearDown() {}

constexpr uint32_t kAp = 0xC0A80401;   // 192.168.4.1
constexpr uint32_t kSta = 0xC0A8010A;  // 192.168.1.10

void test_on_serves_everywhere() {
  TEST_ASSERT_TRUE(webServes(kSta, kAp, true));
  TEST_ASSERT_TRUE(webServes(kAp, kAp, true));
  TEST_ASSERT_TRUE(webServes(kSta, 0, true));
}

void test_off_serves_only_the_setup_ap() {
  TEST_ASSERT_FALSE(webServes(kSta, kAp, false));
  TEST_ASSERT_TRUE(webServes(kAp, kAp, false));
}

void test_off_with_the_ap_down_serves_nothing() {
  TEST_ASSERT_FALSE(webServes(kSta, 0, false));
  TEST_ASSERT_FALSE(webServes(0, 0, false));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_on_serves_everywhere);
  RUN_TEST(test_off_serves_only_the_setup_ap);
  RUN_TEST(test_off_with_the_ap_down_serves_nothing);
  return UNITY_END();
}
