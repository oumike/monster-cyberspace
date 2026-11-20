#include <unity.h>

#include <string>

#include "ota/ota_logic.h"

using namespace rc;

void setUp() {}
void tearDown() {}

void test_compare_versions() {
  TEST_ASSERT_EQUAL(0, compareVersions("0.1.0", "v0.1.0"));
  TEST_ASSERT_EQUAL(-1, compareVersions("0.1.0", "v0.1.1"));
  TEST_ASSERT_EQUAL(1, compareVersions("v0.2.0", "v0.1.9"));
  TEST_ASSERT_EQUAL(-1, compareVersions("0.1.9", "0.1.10"));  // numeric, not text
  TEST_ASSERT_EQUAL(1, compareVersions("1.0", "0.9.9"));
  TEST_ASSERT_EQUAL(0, compareVersions("1.0", "1.0.0"));
}

void test_build_metadata_never_newer() {
  // A dist-only build of a release is not offered that same release.
  TEST_ASSERT_EQUAL(0, compareVersions("0.1.0+abc1234.custom", "v0.1.0"));
  TEST_ASSERT_EQUAL(-1, compareVersions("0.1.0+abc1234", "v0.1.1"));
  TEST_ASSERT_EQUAL(0, compareVersions("0.1.0-rc1", "0.1.0"));
}

void test_tag_from_location() {
  TEST_ASSERT_EQUAL_STRING(
      "v0.1.2",
      tagFromLocation("https://github.com/oumike/monster-cyberspace/releases/tag/v0.1.2").c_str());
  TEST_ASSERT_EQUAL_STRING("v10.0.31", tagFromLocation("/oumike/x/releases/tag/v10.0.31").c_str());
}

void test_tag_from_location_rejects() {
  // No release yet: GitHub sends /releases instead.
  TEST_ASSERT_EQUAL_STRING("",
                           tagFromLocation("https://github.com/oumike/monster-cyberspace/releases").c_str());
  TEST_ASSERT_EQUAL_STRING("", tagFromLocation("").c_str());
  TEST_ASSERT_EQUAL_STRING("", tagFromLocation("https://x/releases/tag/nightly").c_str());
  TEST_ASSERT_EQUAL_STRING("", tagFromLocation("https://x/releases/tag/v1.2").c_str());
  TEST_ASSERT_EQUAL_STRING("", tagFromLocation("https://x/releases/tag/v1.2.3/../evil").c_str());
  TEST_ASSERT_EQUAL_STRING("", tagFromLocation("https://x/releases/tag/v1.2.3?x=1").c_str());
}

void test_asset_urls() {
  TEST_ASSERT_EQUAL_STRING("monster-cyberspace-pager-v0.1.2-ota.bin", otaAssetName("pager", "v0.1.2").c_str());
  TEST_ASSERT_EQUAL_STRING(
      "https://github.com/oumike/monster-cyberspace/releases/download/v0.1.2/"
      "monster-cyberspace-wio-l2-v0.1.2-ota.bin",
      otaAssetUrl("wio-l2", "v0.1.2").c_str());
  TEST_ASSERT_EQUAL_STRING("https://github.com/oumike/monster-cyberspace/releases/latest", kOtaLatestUrl);
}

void test_launcher_hosted() {
  // Our 16 MB table: ota_0 and ota_1, running from either.
  const AppSlot ours[] = {{0x10000, 0x10}, {0x650000, 0x11}};
  TEST_ASSERT_FALSE(launcherHosted(ours, 2, 0x10000));
  TEST_ASSERT_FALSE(launcherHosted(ours, 2, 0x650000));
  // Our 8 MB table: one factory app, the one running.
  const AppSlot factory[] = {{0x10000, 0x00}};
  TEST_ASSERT_FALSE(launcherHosted(factory, 1, 0x10000));
  // Launcher's tables: itself in test (ESP32-S3) or factory (ESP32-P4) at
  // 0x10000, apps in OTA slots after it.
  const AppSlot s3[] = {{0x10000, 0x20}, {0x1a0000, 0x10}, {0x520000, 0x11}};
  TEST_ASSERT_TRUE(launcherHosted(s3, 3, 0x520000));
  const AppSlot p4[] = {{0x10000, 0x00}, {0x1a0000, 0x10}};
  TEST_ASSERT_TRUE(launcherHosted(p4, 2, 0x1a0000));
  // Launcher itself running.
  TEST_ASSERT_FALSE(launcherHosted(s3, 3, 0x10000));
  TEST_ASSERT_FALSE(launcherHosted(nullptr, 0, 0x10000));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_compare_versions);
  RUN_TEST(test_build_metadata_never_newer);
  RUN_TEST(test_tag_from_location);
  RUN_TEST(test_tag_from_location_rejects);
  RUN_TEST(test_asset_urls);
  RUN_TEST(test_launcher_hosted);
  return UNITY_END();
}
