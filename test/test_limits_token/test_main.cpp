#include <unity.h>

#include <string>

#include "cs/rate_limiter.h"
#include "cs/token.h"

using namespace rc;

void setUp() {}
void tearDown() {}

void test_circ_minute() {
  RateLimiter l = makeCircSendLimiter();
  TEST_ASSERT_EQUAL(0, l.retryAfterMs(0));
  for (int i = 0; i < 15; ++i) TEST_ASSERT_TRUE(l.tryAcquire(i * 1000));
  TEST_ASSERT_FALSE(l.tryAcquire(15000));
  TEST_ASSERT_EQUAL(45000, l.retryAfterMs(15000));
  TEST_ASSERT_TRUE(l.tryAcquire(60001));
}

void test_circ_hourly() {
  RateLimiter l = makeCircSendLimiter();
  for (int i = 0; i < 150; ++i) TEST_ASSERT_TRUE(l.tryAcquire(int64_t(i) * 5000));
  TEST_ASSERT_FALSE(l.tryAcquire(150 * 5000));
  // The first send leaves the hour window at 3,600,000.
  TEST_ASSERT_EQUAL(3600000 - 750000, l.retryAfterMs(750000));
}

void test_presence_and_delete_limits() {
  RateLimiter p = makePresenceLimiter();
  for (int i = 0; i < 15; ++i) TEST_ASSERT_TRUE(p.tryAcquire(i));
  TEST_ASSERT_FALSE(p.tryAcquire(20));
  RateLimiter d = makeDeleteLimiter();
  for (int i = 0; i < 30; ++i) TEST_ASSERT_TRUE(d.tryAcquire(int64_t(i) * 60000));
  TEST_ASSERT_FALSE(d.tryAcquire(30LL * 60000));
}

static std::string jwt(const char* payload) {
  return std::string("eyJhbGciOiJSUzI1NiJ9.") + payload + ".sig";
}

void test_jwt_exp() {
  TEST_ASSERT_TRUE(jwtExpSeconds(jwt("eyJzdWIiOiJ1MSIsImV4cCI6MTc5Mjk2MDAwMH0")) ==
                   1792960000LL);
  // Base64url alphabet ('-', '_') with no padding.
  TEST_ASSERT_TRUE(jwtExpSeconds(jwt("eyJleHAiOjE3OTI5NjAwMDAsIngiOiI_Pz4-fn4ifQ")) ==
                   1792960000LL);
  TEST_ASSERT_EQUAL(-1, jwtExpSeconds("garbage"));
  TEST_ASSERT_EQUAL(-1, jwtExpSeconds("a.!!!.b"));
  TEST_ASSERT_EQUAL(-1, jwtExpSeconds(jwt("e30")));  // {} has no exp
}

void test_refresh_synced() {
  TEST_ASSERT_TRUE(refreshDueMonoMs(1000, 1792956400, 1792960000) == 1000 + 3300 * 1000);
}

void test_refresh_unsynced() {
  TEST_ASSERT_TRUE(refreshDueMonoMs(1000, 0, 1792960000) == 1000 + 3300000);
  TEST_ASSERT_TRUE(refreshDueMonoMs(1000, 1792956400, -1) == 1000 + 3300000);
}

void test_refresh_floor() {
  TEST_ASSERT_TRUE(refreshDueMonoMs(1000, 1792959900, 1792960000) == 1000 + 60000);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_circ_minute);
  RUN_TEST(test_circ_hourly);
  RUN_TEST(test_presence_and_delete_limits);
  RUN_TEST(test_jwt_exp);
  RUN_TEST(test_refresh_synced);
  RUN_TEST(test_refresh_unsynced);
  RUN_TEST(test_refresh_floor);
  return UNITY_END();
}
