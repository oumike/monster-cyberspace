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

void test_cmail_limits() {
  RateLimiter m = makeCmailSendLimiter();
  for (int i = 0; i < 15; ++i) TEST_ASSERT_TRUE(m.tryAcquire(i));
  TEST_ASSERT_FALSE(m.tryAcquire(20));
  RateLimiter c = makeStartConversationLimiter();
  for (int i = 0; i < 5; ++i) TEST_ASSERT_TRUE(c.tryAcquire(i));
  TEST_ASSERT_FALSE(c.tryAcquire(10));
  RateLimiter h = makeStartConversationLimiter();
  for (int i = 0; i < 30; ++i) TEST_ASSERT_TRUE(h.tryAcquire(int64_t(i) * 60000));
  TEST_ASSERT_FALSE(h.tryAcquire(30LL * 60000));  // 30 per hour
  RateLimiter t = makeTypingLimiter();
  for (int i = 0; i < 40; ++i) TEST_ASSERT_TRUE(t.tryAcquire(i));
  TEST_ASSERT_FALSE(t.tryAcquire(50));
}

static void checkMinuteEdge(RateLimiter l, int perMin) {
  for (int i = 0; i < perMin; ++i) TEST_ASSERT_TRUE(l.tryAcquire(0));
  TEST_ASSERT_FALSE(l.tryAcquire(0));
  TEST_ASSERT_FALSE(l.tryAcquire(59999));
  TEST_ASSERT_TRUE(l.tryAcquire(60000));
}

static void checkDayEdge(RateLimiter l, int perDay) {
  // One acquire every 30 minutes stays under every per-minute limit.
  for (int i = 0; i < perDay; ++i) TEST_ASSERT_TRUE(l.tryAcquire(int64_t(i) * 1800000));
  int64_t after = int64_t(perDay) * 1800000;
  if (after < 86400000) TEST_ASSERT_FALSE(l.tryAcquire(after));
  TEST_ASSERT_FALSE(l.tryAcquire(86399999));
  TEST_ASSERT_TRUE(l.tryAcquire(86400000));
}

void test_feed_limiters() {
  checkMinuteEdge(makeEntryLimiter(), 2);
  checkDayEdge(makeEntryLimiter(), 24);
  checkMinuteEdge(makeReplyLimiter(), 3);
  checkMinuteEdge(makeBookmarkLimiter(), 5);
  checkMinuteEdge(makeWatchLimiter(), 10);
  checkMinuteEdge(makeMembershipLimiter(), 3);
  checkDayEdge(makeMembershipLimiter(), 15);
  RateLimiter r = makeReplyLimiter();
  for (int i = 0; i < 48; ++i) TEST_ASSERT_TRUE(r.tryAcquire(int64_t(i) * 1200000));
  TEST_ASSERT_FALSE(r.tryAcquire(48LL * 1200000 - 1));
  TEST_ASSERT_TRUE(r.tryAcquire(86400000));
  RateLimiter b = makeBookmarkLimiter();
  for (int i = 0; i < 75; ++i) TEST_ASSERT_TRUE(b.tryAcquire(int64_t(i) * 1000000));
  TEST_ASSERT_FALSE(b.tryAcquire(75000000));
  TEST_ASSERT_TRUE(b.tryAcquire(86400000));
  RateLimiter w = makeWatchLimiter();
  for (int i = 0; i < 100; ++i) TEST_ASSERT_TRUE(w.tryAcquire(int64_t(i) * 800000));
  TEST_ASSERT_FALSE(w.tryAcquire(80000000));
  TEST_ASSERT_TRUE(w.tryAcquire(86400000));
  checkMinuteEdge(makeFollowLimiter(), 3);
  checkDayEdge(makeFollowLimiter(), 15);
  RateLimiter k = makePokeLimiter();
  TEST_ASSERT_TRUE(k.tryAcquire(0));
  TEST_ASSERT_FALSE(k.tryAcquire(3599999));
  for (int i = 1; i < 8; ++i) TEST_ASSERT_TRUE(k.tryAcquire(int64_t(i) * 3600000));
  TEST_ASSERT_FALSE(k.tryAcquire(8LL * 3600000));
  TEST_ASSERT_TRUE(k.tryAcquire(86400000));
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
  RUN_TEST(test_cmail_limits);
  RUN_TEST(test_feed_limiters);
  return UNITY_END();
}
