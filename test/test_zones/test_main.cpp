#include <unity.h>

#include <cstring>
#include <string>

#include "tz/zones.h"

using namespace rc;

void setUp() {}
void tearDown() {}

void test_table_sorted_unique() {
  TEST_ASSERT_TRUE(zoneCount() >= 40);
  for (size_t i = 1; i < zoneCount(); ++i)
    TEST_ASSERT_TRUE_MESSAGE(strcmp(zoneAt(i - 1).iana, zoneAt(i).iana) < 0, zoneAt(i).iana);
}

void test_entries_valid() {
  for (size_t i = 0; i < zoneCount(); ++i) {
    const Zone& z = zoneAt(i);
    TEST_ASSERT_TRUE_MESSAGE(*z.label, z.iana);
    TEST_ASSERT_TRUE_MESSAGE(strlen(z.label) <= kMaxZoneLabel, z.iana);
    TEST_ASSERT_TRUE_MESSAGE(validPosixTz(z.posix), z.iana);
  }
}

void test_find_by_iana() {
  const Zone* z = findZoneByIana("America/New_York");
  TEST_ASSERT_NOT_NULL(z);
  TEST_ASSERT_EQUAL_STRING("EST5EDT,M3.2.0,M11.1.0", z->posix);
  z = findZoneByIana("Etc/UTC");
  TEST_ASSERT_NOT_NULL(z);
  TEST_ASSERT_EQUAL_STRING("UTC0", z->posix);
  TEST_ASSERT_NULL(findZoneByIana("Mars/Olympus_Mons"));
  TEST_ASSERT_NULL(findZoneByIana(""));
  TEST_ASSERT_NULL(findZoneByIana("America/New_Yor"));
}

void test_find_by_posix_first_match() {
  // New York and Toronto share a rule; New York comes first.
  const Zone* z = findZoneByPosix("EST5EDT,M3.2.0,M11.1.0");
  TEST_ASSERT_NOT_NULL(z);
  TEST_ASSERT_EQUAL_STRING("America/New_York", z->iana);
  TEST_ASSERT_NULL(findZoneByPosix("XYZ3"));
  TEST_ASSERT_NULL(findZoneByPosix(""));
}

void test_zone_label() {
  TEST_ASSERT_EQUAL_STRING(findZoneByIana("Asia/Tokyo")->label, zoneLabel("Asia/Tokyo"));
  TEST_ASSERT_EQUAL_STRING("Custom", zoneLabel(""));
  TEST_ASSERT_EQUAL_STRING("Custom", zoneLabel("Nowhere/Else"));
}

void test_valid_posix_tz() {
  TEST_ASSERT_TRUE(validPosixTz("UTC0"));
  TEST_ASSERT_TRUE(validPosixTz("<+0545>-5:45"));
  TEST_ASSERT_FALSE(validPosixTz(""));
  TEST_ASSERT_FALSE(validPosixTz(std::string(64, 'A')));
  TEST_ASSERT_TRUE(validPosixTz(std::string(63, 'A')));
  TEST_ASSERT_FALSE(validPosixTz("EST5\nEDT"));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_table_sorted_unique);
  RUN_TEST(test_entries_valid);
  RUN_TEST(test_find_by_iana);
  RUN_TEST(test_find_by_posix_first_match);
  RUN_TEST(test_zone_label);
  RUN_TEST(test_valid_posix_tz);
  return UNITY_END();
}
