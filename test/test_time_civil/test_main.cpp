#include <unity.h>

#include "time/civil.h"
#include "time/pcf8563.h"

using namespace rc;

void setUp() {}
void tearDown() {}

void test_days_from_civil() {
  TEST_ASSERT_EQUAL_INT64(0, daysFromCivil(1970, 1, 1));
  TEST_ASSERT_EQUAL_INT64(20752, daysFromCivil(2026, 10, 26));
  int y, m, d;
  civilFromDays(daysFromCivil(2028, 2, 29), y, m, d);
  TEST_ASSERT_EQUAL(2028, y);
  TEST_ASSERT_EQUAL(2, m);
  TEST_ASSERT_EQUAL(29, d);
}

void test_pcf_encode_decode() {
  uint8_t r[7];
  pcf8563Encode(1793039400, r);  // Mon 2026-10-26 18:30:00Z
  const uint8_t want[7] = {0x00, 0x30, 0x18, 0x26, 0x01, 0x10, 0x26};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(want, r, 7);
  int64_t e;
  TEST_ASSERT_TRUE(pcf8563Decode(r, e));
  TEST_ASSERT_EQUAL_INT64(1793039400, e);
}

void test_pcf_vl_flag() {
  uint8_t r[7];
  pcf8563Encode(1793039400, r);
  r[0] |= 0x80;
  int64_t e;
  TEST_ASSERT_FALSE(pcf8563Decode(r, e));
}

void test_pcf_bad_bcd() {
  uint8_t r[7];
  int64_t e;
  pcf8563Encode(1793039400, r);
  r[5] = 0x13;  // month 13
  TEST_ASSERT_FALSE(pcf8563Decode(r, e));
  pcf8563Encode(1793039400, r);
  r[0] = 0x5A;  // not BCD
  TEST_ASSERT_FALSE(pcf8563Decode(r, e));
  pcf8563Encode(1793039400, r);
  r[3] = 0x00;  // day 0
  TEST_ASSERT_FALSE(pcf8563Decode(r, e));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_days_from_civil);
  RUN_TEST(test_pcf_encode_decode);
  RUN_TEST(test_pcf_vl_flag);
  RUN_TEST(test_pcf_bad_bcd);
  return UNITY_END();
}
