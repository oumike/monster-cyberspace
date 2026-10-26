#include <unity.h>

#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include "../../src/tz/tz_data.h"
#include "../fixtures/tz_fixture_blob.h"
#include "tz/tz_lookup.h"

using namespace rc;

void setUp() {}
void tearDown() {}

static const uint8_t* fixture() { return fx::kTzFixture; }

static bool look(double lat, double lon, TzZone& z) {
  return tzLookup(fixture(), fx::kTzFixtureSize, lat, lon, z);
}

static uint32_t u32At(const std::vector<uint8_t>& b, size_t off) {
  uint32_t v;
  memcpy(&v, b.data() + off, 4);
  return v;
}

void test_blob_valid() {
  TEST_ASSERT_TRUE(tzBlobValid(fixture(), fx::kTzFixtureSize));
  std::vector<uint8_t> bad(fixture(), fixture() + fx::kTzFixtureSize);
  bad[0] = 'X';
  TEST_ASSERT_FALSE(tzBlobValid(bad.data(), bad.size()));
  TEST_ASSERT_FALSE(tzBlobValid(fixture(), 20));
}

void test_lookup_inside() {
  TzZone z;
  TEST_ASSERT_TRUE(look(42.5, -7.5, z));
  TEST_ASSERT_EQUAL_STRING("Test/West", z.name.c_str());
  TEST_ASSERT_EQUAL_STRING("<-01>1", z.posix.c_str());
  TEST_ASSERT_TRUE(look(42.5, -2.5, z));
  TEST_ASSERT_EQUAL_STRING("Test/East", z.name.c_str());
  TEST_ASSERT_EQUAL_STRING("<+02>-2", z.posix.c_str());
}

void test_lookup_outside() {
  TzZone z;
  TEST_ASSERT_FALSE(look(50, -7.5, z));
  TEST_ASSERT_EQUAL_STRING("Etc/GMT", z.name.c_str());
  TEST_ASSERT_EQUAL_STRING("GMT0", z.posix.c_str());
}

void test_lookup_on_edge() {
  TzZone z;
  TEST_ASSERT_TRUE(look(42.5, -5.0, z));
  TEST_ASSERT_EQUAL_STRING("Test/East", z.name.c_str());
}

void test_lookup_hole() {
  TzZone z;
  TEST_ASSERT_FALSE(look(-5, 15, z));
  TEST_ASSERT_EQUAL_STRING("Etc/GMT-1", z.name.c_str());
  TEST_ASSERT_EQUAL_STRING("<+01>-1", z.posix.c_str());
  TEST_ASSERT_TRUE(look(-2, 11, z));
  TEST_ASSERT_EQUAL_STRING("Test/Hole", z.name.c_str());
  TEST_ASSERT_EQUAL_STRING("<+05>-5", z.posix.c_str());
}

void test_ocean_both_hemispheres() {
  TzZone z = oceanZone(-75);
  TEST_ASSERT_EQUAL_STRING("Etc/GMT+5", z.name.c_str());
  TEST_ASSERT_EQUAL_STRING("<-05>5", z.posix.c_str());
  z = oceanZone(135);
  TEST_ASSERT_EQUAL_STRING("Etc/GMT-9", z.name.c_str());
  TEST_ASSERT_EQUAL_STRING("<+09>-9", z.posix.c_str());
  z = oceanZone(180);
  TEST_ASSERT_EQUAL_STRING("Etc/GMT-12", z.name.c_str());
  TEST_ASSERT_EQUAL_STRING("<+12>-12", z.posix.c_str());
  z = oceanZone(-180);
  TEST_ASSERT_EQUAL_STRING("Etc/GMT+12", z.name.c_str());
  TEST_ASSERT_EQUAL_STRING("<-12>12", z.posix.c_str());
}

void test_lookup_extremes() {
  const double lats[] = {90, -90};
  const double lons[] = {180, -180};
  for (double lat : lats) {
    for (double lon : lons) {
      TzZone z;
      TEST_ASSERT_FALSE(look(lat, lon, z));
      TEST_ASSERT_EQUAL_STRING(oceanZone(lon).name.c_str(), z.name.c_str());
      TEST_ASSERT_EQUAL_STRING(oceanZone(lon).posix.c_str(), z.posix.c_str());
    }
  }
}

void test_lookup_corrupt_blob() {
  std::vector<uint8_t> b(fixture(), fixture() + fx::kTzFixtureSize);
  uint16_t count;
  memcpy(&count, b.data() + 6, 2);
  uint32_t zonesOff = u32At(b, 8);
  bool patched = false;
  for (uint16_t i = 0; i < count; ++i) {
    size_t rec = zonesOff + 16u * i;
    if (strcmp(reinterpret_cast<const char*>(b.data() + u32At(b, rec)), "Test/West") == 0) {
      uint32_t bad = 0xFFFFFF00u;
      memcpy(b.data() + rec + 8, &bad, 4);
      patched = true;
    }
  }
  TEST_ASSERT_TRUE(patched);
  TzZone z;
  // Row 40 holds Test/West's southern edge, so the lookup reads its rings.
  TEST_ASSERT_TRUE(tzLookup(fixture(), fx::kTzFixtureSize, 40.5, -7.5, z));
  TEST_ASSERT_FALSE(tzLookup(b.data(), b.size(), 40.5, -7.5, z));
  TEST_ASSERT_EQUAL_STRING("Etc/GMT", z.name.c_str());
  TEST_ASSERT_TRUE(tzLookup(b.data(), b.size(), 42.5, -2.5, z));
  TEST_ASSERT_EQUAL_STRING("Test/East", z.name.c_str());
}

void test_tracker_10km() {
  const double kmPerDegLat = 6371.0 * M_PI / 180.0;
  TzTracker t;
  TzZone z;
  TEST_ASSERT_TRUE(t.update(fixture(), fx::kTzFixtureSize, 42.5, -7.5, z));
  TEST_ASSERT_EQUAL_STRING("Test/West", z.name.c_str());
  z.name = "unchanged";
  TEST_ASSERT_FALSE(t.update(fixture(), fx::kTzFixtureSize, 42.5 + 5 / kmPerDegLat, -7.5, z));
  TEST_ASSERT_EQUAL_STRING("unchanged", z.name.c_str());
  TEST_ASSERT_TRUE(t.update(fixture(), fx::kTzFixtureSize, 42.5 + 11 / kmPerDegLat, -7.5, z));
  TEST_ASSERT_EQUAL_STRING("Test/West", z.name.c_str());
  t.reset();
  TEST_ASSERT_TRUE(t.update(fixture(), fx::kTzFixtureSize, 42.5 + 11 / kmPerDegLat, -7.5, z));
}

void test_distance_km() {
  TEST_ASSERT_TRUE(fabs(distanceKm(0, 0, 0, 0)) < 1e-9);
  TEST_ASSERT_TRUE(fabs(distanceKm(0, 0, 1, 0) - 111.195) < 0.01);
}

void test_effective_tz_and_mode_keys() {
  const std::string manual = "CET-1CEST,M3.5.0,M10.5.0/3";
  const std::string none;
  const std::string autoTz = "<+02>-2";
  TEST_ASSERT_EQUAL_STRING(manual.c_str(), effectiveTz(TzMode::Automatic, manual, none).c_str());
  TEST_ASSERT_EQUAL_STRING(autoTz.c_str(), effectiveTz(TzMode::Automatic, manual, autoTz).c_str());
  TEST_ASSERT_EQUAL_STRING(manual.c_str(), effectiveTz(TzMode::Manual, manual, autoTz).c_str());
  TEST_ASSERT_EQUAL_STRING("auto", tzModeKey(TzMode::Automatic));
  TEST_ASSERT_EQUAL_STRING("manual", tzModeKey(TzMode::Manual));
  TzMode m = TzMode::Manual;
  TEST_ASSERT_TRUE(parseTzMode("auto", m));
  TEST_ASSERT_TRUE(m == TzMode::Automatic);
  TEST_ASSERT_TRUE(parseTzMode("manual", m));
  TEST_ASSERT_TRUE(m == TzMode::Manual);
  TEST_ASSERT_FALSE(parseTzMode("x", m));
  TEST_ASSERT_TRUE(m == TzMode::Manual);
}

void test_real_data() {
  TEST_ASSERT_TRUE(tzBlobValid(kTzData, kTzDataSize));
  struct Place {
    double lat, lon;
    const char* name;
  };
  const Place places[] = {
      {40.7128, -74.006, "America/New_York"}, {51.5074, -0.1278, "Europe/London"},
      {48.8566, 2.3522, "Europe/Paris"},      {35.6762, 139.6503, "Asia/Tokyo"},
      {-33.8688, 151.2093, "Australia/Sydney"},
  };
  TzZone z;
  for (const Place& p : places) {
    TEST_ASSERT_TRUE_MESSAGE(tzLookup(kTzData, kTzDataSize, p.lat, p.lon, z), p.name);
    TEST_ASSERT_EQUAL_STRING(p.name, z.name.c_str());
  }
  TEST_ASSERT_TRUE(tzLookup(kTzData, kTzDataSize, 40.7128, -74.006, z));
  TEST_ASSERT_EQUAL_STRING("EST5EDT,M3.2.0,M11.1.0", z.posix.c_str());
  TEST_ASSERT_FALSE(tzLookup(kTzData, kTzDataSize, 30, -40, z));
  TEST_ASSERT_EQUAL_STRING("Etc/GMT+3", z.name.c_str());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_blob_valid);
  RUN_TEST(test_lookup_inside);
  RUN_TEST(test_lookup_outside);
  RUN_TEST(test_lookup_on_edge);
  RUN_TEST(test_lookup_hole);
  RUN_TEST(test_ocean_both_hemispheres);
  RUN_TEST(test_lookup_extremes);
  RUN_TEST(test_lookup_corrupt_blob);
  RUN_TEST(test_tracker_10km);
  RUN_TEST(test_distance_km);
  RUN_TEST(test_effective_tz_and_mode_keys);
  RUN_TEST(test_real_data);
  return UNITY_END();
}
