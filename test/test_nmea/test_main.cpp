#include <unity.h>

#include <cstdio>
#include <cstring>
#include <string>

#include "gps/nmea.h"

using namespace rc;

void setUp() {}
void tearDown() {}

static std::string nmea(const char* body) {
  uint8_t x = 0;
  for (const char* p = body; *p; ++p) x ^= static_cast<uint8_t>(*p);
  char hex[3];
  snprintf(hex, sizeof hex, "%02X", x);
  return std::string("$") + body + "*" + hex + "\r\n";
}

static void feed(NmeaParser& p, const std::string& s, int64_t nowMs) {
  p.feed(reinterpret_cast<const uint8_t*>(s.data()), s.size(), nowMs);
}

static std::string replace(std::string s, const char* from, const char* to) {
  s.replace(s.find(from), strlen(from), to);
  return s;
}

const char* kRmc = "GNRMC,183000.00,A,4042.768,N,07400.360,W,0.0,0.0,261026,,,A";

void test_rmc_sets_time() {
  NmeaParser p;
  feed(p, nmea(kRmc), 5000);
  const GpsState& s = p.state();
  TEST_ASSERT_TRUE(s.utcValid);
  TEST_ASSERT_EQUAL_INT64(1793039400000LL, s.utcEpochMs);
  TEST_ASSERT_EQUAL_INT64(5000, s.utcAtMs);
  TEST_ASSERT_TRUE(s.fix);
  TEST_ASSERT_EQUAL_INT64(5000, s.lastFixMs);
}

void test_rmc_fraction_ms() {
  NmeaParser p;
  feed(p, nmea(replace(kRmc, "183000.00", "183000.50").c_str()), 1);
  TEST_ASSERT_TRUE(p.state().utcValid);
  TEST_ASSERT_EQUAL_INT64(1793039400500LL, p.state().utcEpochMs);
}

void test_gga_sats_used() {
  NmeaParser p;
  feed(p, nmea("GNGGA,183000.00,4042.768,N,07400.360,W,1,07,1.0,10.0,M,-34.0,M,,"), 1);
  TEST_ASSERT_EQUAL(7, p.state().satsUsed);
}

void test_gsv_summed_over_talkers() {
  NmeaParser p;
  feed(p, nmea("GPGSV,2,1,08,01,40,083,46,02,17,308,41,12,07,344,39,14,22,228,45"), 1);
  feed(p, nmea("GPGSV,2,2,08,15,40,083,46,16,17,308,41,17,07,344,39,18,22,228,45"), 2);
  feed(p, nmea("GLGSV,1,1,03,65,40,083,46,66,17,308,41,67,07,344,39"), 3);
  TEST_ASSERT_EQUAL(11, p.state().satsInView);
  // A later group from the same talker replaces, not adds.
  feed(p, nmea("GPGSV,1,1,04,01,40,083,46,02,17,308,41,12,07,344,39,14,22,228,45"), 4);
  TEST_ASSERT_EQUAL(7, p.state().satsInView);
}

void test_bad_checksum_ignored() {
  NmeaParser p;
  std::string s = nmea(kRmc);
  size_t star = s.find('*');
  s[star + 1] = s[star + 1] == '0' ? '1' : '0';
  feed(p, s, 1);
  TEST_ASSERT_FALSE(p.state().utcValid);
  // No checksum at all is dropped too.
  feed(p, std::string("$") + kRmc + "\r\n", 2);
  TEST_ASSERT_FALSE(p.state().utcValid);
}

void test_rmc_void_not_valid() {
  NmeaParser p;
  feed(p, nmea(replace(kRmc, ",A,", ",V,").c_str()), 1);
  TEST_ASSERT_FALSE(p.state().fix);
  TEST_ASSERT_FALSE(p.state().utcValid);
  // After a valid one, V clears fix but keeps the UTC.
  feed(p, nmea(kRmc), 2);
  feed(p, nmea(replace(kRmc, ",A,", ",V,").c_str()), 3);
  TEST_ASSERT_FALSE(p.state().fix);
  TEST_ASSERT_TRUE(p.state().utcValid);
  TEST_ASSERT_EQUAL_INT64(2, p.state().utcAtMs);
  TEST_ASSERT_EQUAL_INT64(2, p.state().lastFixMs);
}

void test_rmc_without_position() {
  NmeaParser p;
  feed(p, nmea(replace(kRmc, "4042.768,N,07400.360,W", ",,,").c_str()), 1);
  TEST_ASSERT_TRUE(p.state().fix);
  TEST_ASSERT_TRUE(p.state().utcValid);
  TEST_ASSERT_EQUAL_INT64(1793039400000LL, p.state().utcEpochMs);
}

void test_rmc_empty_date_not_valid() {
  NmeaParser p;
  feed(p, nmea(replace(kRmc, ",261026,", ",,").c_str()), 1);
  TEST_ASSERT_FALSE(p.state().utcValid);
}

void test_split_across_reads() {
  NmeaParser whole, split;
  std::string s = nmea(kRmc);
  feed(whole, s, 5000);
  for (char c : s) {
    uint8_t b = static_cast<uint8_t>(c);
    split.feed(&b, 1, 5000);
  }
  TEST_ASSERT_TRUE(split.state().utcValid);
  TEST_ASSERT_EQUAL_INT64(whole.state().utcEpochMs, split.state().utcEpochMs);
  TEST_ASSERT_EQUAL_INT64(whole.state().utcAtMs, split.state().utcAtMs);
  TEST_ASSERT_EQUAL(whole.state().fix, split.state().fix);
}

void test_unknown_and_talkers() {
  NmeaParser p;
  feed(p, nmea("GPTXT,01,01,02,ANTENNA OK"), 1);
  TEST_ASSERT_FALSE(p.state().utcValid);
  TEST_ASSERT_FALSE(p.state().fix);
  TEST_ASSERT_EQUAL(0, p.state().satsUsed);
  TEST_ASSERT_EQUAL(0, p.state().satsInView);
  feed(p, nmea(replace(kRmc, "GNRMC", "BDRMC").c_str()), 5000);
  TEST_ASSERT_TRUE(p.state().utcValid);
  TEST_ASSERT_EQUAL_INT64(1793039400000LL, p.state().utcEpochMs);
}

void test_garbage_and_overlong_resync() {
  NmeaParser p;
  std::string junk(200, '\xFF');
  junk += "$GPXXX";
  junk += std::string(120, 'A');
  feed(p, junk, 1);
  feed(p, nmea(kRmc), 2);
  TEST_ASSERT_TRUE(p.state().utcValid);
  TEST_ASSERT_EQUAL_INT64(2, p.state().utcAtMs);
}

void test_reset_clears() {
  NmeaParser p;
  feed(p, nmea(kRmc), 1);
  feed(p, nmea("GLGSV,1,1,03,65,40,083,46,66,17,308,41,67,07,344,39"), 2);
  p.reset();
  TEST_ASSERT_FALSE(p.state().fix);
  TEST_ASSERT_FALSE(p.state().utcValid);
  TEST_ASSERT_EQUAL(0, p.state().satsInView);
  TEST_ASSERT_EQUAL_INT64(-1, p.state().utcAtMs);
  // The per-talker counts are gone too.
  feed(p, nmea("GPGSV,1,1,02,01,40,083,46,02,17,308,41"), 3);
  TEST_ASSERT_EQUAL(2, p.state().satsInView);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_rmc_sets_time);
  RUN_TEST(test_rmc_fraction_ms);
  RUN_TEST(test_gga_sats_used);
  RUN_TEST(test_gsv_summed_over_talkers);
  RUN_TEST(test_bad_checksum_ignored);
  RUN_TEST(test_rmc_void_not_valid);
  RUN_TEST(test_rmc_without_position);
  RUN_TEST(test_rmc_empty_date_not_valid);
  RUN_TEST(test_split_across_reads);
  RUN_TEST(test_unknown_and_talkers);
  RUN_TEST(test_garbage_and_overlong_resync);
  RUN_TEST(test_reset_clears);
  return UNITY_END();
}
