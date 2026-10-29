#include <unity.h>

#include "net/wifi_list.h"

using namespace rc;

void setUp() {}
void tearDown() {}

void test_bars_and_validation() {
  TEST_ASSERT_EQUAL(4, wifiBarsFor(-50));
  TEST_ASSERT_EQUAL(3, wifiBarsFor(-65));
  TEST_ASSERT_EQUAL(2, wifiBarsFor(-70));
  TEST_ASSERT_EQUAL(1, wifiBarsFor(-85));
  TEST_ASSERT_EQUAL(0, wifiBarsFor(-90));
  TEST_ASSERT_FALSE(validWifiSsid(""));
  TEST_ASSERT_TRUE(validWifiSsid("home"));
  TEST_ASSERT_TRUE(validWifiSsid(std::string(32, 'a')));
  TEST_ASSERT_FALSE(validWifiSsid(std::string(33, 'a')));
  TEST_ASSERT_TRUE(validWifiPassword(""));  // an open network
  TEST_ASSERT_FALSE(validWifiPassword("short"));
  TEST_ASSERT_TRUE(validWifiPassword("12345678"));
  TEST_ASSERT_TRUE(validWifiPassword(std::string(63, 'p')));
  TEST_ASSERT_FALSE(validWifiPassword(std::string(64, 'p')));
}

void test_save_updates_refuses_when_full_and_forgets() {
  std::vector<WifiNet> nets;
  TEST_ASSERT(saveWifiNet(nets, "a", "password1") == WifiSave::Added);
  TEST_ASSERT(saveWifiNet(nets, "a", "password2") == WifiSave::Updated);
  TEST_ASSERT_EQUAL(1, nets.size());
  TEST_ASSERT_EQUAL_STRING("password2", nets[0].password.c_str());
  for (const char* s : {"b", "c", "d", "e"}) saveWifiNet(nets, s, "");
  TEST_ASSERT(saveWifiNet(nets, "f", "") == WifiSave::Full);
  TEST_ASSERT_EQUAL(kMaxWifiNets, nets.size());
  TEST_ASSERT(saveWifiNet(nets, "c", "password3") == WifiSave::Updated);  // still allowed
  TEST_ASSERT_TRUE(forgetWifiNet(nets, "c"));
  TEST_ASSERT_FALSE(forgetWifiNet(nets, "c"));
  TEST_ASSERT_EQUAL(4, nets.size());
}

void test_scan_round_trips_any_name() {
  std::vector<WifiScanEntry> in{{"home", -40, true}, {"a,b:c\n9", -80, false}, {"\xf0\x9f\x93\xb6", -60, true}};
  std::vector<WifiScanEntry> out;
  TEST_ASSERT_TRUE(decodeWifiScan(encodeWifiScan(in), out));
  TEST_ASSERT_EQUAL(3, out.size());
  for (size_t i = 0; i < in.size(); ++i) {
    TEST_ASSERT_EQUAL_STRING(in[i].ssid.c_str(), out[i].ssid.c_str());
    TEST_ASSERT_EQUAL(in[i].rssi, out[i].rssi);
    TEST_ASSERT_EQUAL(in[i].secure, out[i].secure);
  }
  TEST_ASSERT_TRUE(decodeWifiScan("", out));
  TEST_ASSERT_EQUAL(0, out.size());
  TEST_ASSERT_FALSE(decodeWifiScan("-40,1,9:short", out));
  TEST_ASSERT_FALSE(decodeWifiScan("junk", out));
}

void test_rows_connected_first_then_strength_then_saved_away() {
  std::vector<WifiScanEntry> scan{{"weak", -88, true}, {"cafe", -50, false}, {"home", -70, true},
                                  {"cafe", -45, false}};
  std::vector<WifiNet> saved{{"office", "password1"}, {"home", "password2"}};
  std::vector<WifiRow> rows = wifiRows(scan, saved, "home");
  TEST_ASSERT_EQUAL(4, rows.size());
  TEST_ASSERT_EQUAL_STRING("home", rows[0].ssid.c_str());
  TEST_ASSERT_TRUE(rows[0].connected);
  TEST_ASSERT_TRUE(rows[0].saved);
  TEST_ASSERT_EQUAL_STRING("cafe", rows[1].ssid.c_str());  // deduplicated, strongest kept
  TEST_ASSERT_EQUAL(4, rows[1].bars);
  TEST_ASSERT_FALSE(rows[1].secure);
  TEST_ASSERT_FALSE(rows[1].saved);
  TEST_ASSERT_EQUAL_STRING("weak", rows[2].ssid.c_str());
  TEST_ASSERT_EQUAL_STRING("office", rows[3].ssid.c_str());  // saved, not in range
  TEST_ASSERT_EQUAL(-1, rows[3].bars);
  TEST_ASSERT_TRUE(rows[3].saved);
  TEST_ASSERT_FALSE(rows[3].connected);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_bars_and_validation);
  RUN_TEST(test_save_updates_refuses_when_full_and_forgets);
  RUN_TEST(test_scan_round_trips_any_name);
  RUN_TEST(test_rows_connected_first_then_strength_then_saved_away);
  return UNITY_END();
}
