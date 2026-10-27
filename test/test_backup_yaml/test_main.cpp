#include <unity.h>

#include <cstring>
#include <string>
#include <vector>

#include "config/backup_yaml.h"
#include "config/yaml_scalar.h"

using namespace rc;

void setUp() {}
void tearDown() {}

namespace {
bool parse(const char* in, std::string& out, ScalarKind& kind) {
  std::string err;
  return yamlScalar(in, out, kind, err);
}
void expectOk(const char* in, const char* want, ScalarKind wantKind) {
  std::string out;
  ScalarKind kind;
  TEST_ASSERT_TRUE_MESSAGE(parse(in, out, kind), in);
  TEST_ASSERT_EQUAL_STRING(want, out.c_str());
  TEST_ASSERT_TRUE(kind == wantKind);
}
}  // namespace

void test_quote_escapes() {
  TEST_ASSERT_EQUAL_STRING("\"a\\\\b\\\"c\\nd\\te\"", yamlQuote("a\\b\"c\nd\te").c_str());
  TEST_ASSERT_EQUAL_STRING("\"\\u0001\\u007F\"", yamlQuote("\x01\x7F").c_str());
  TEST_ASSERT_EQUAL_STRING("\"Café 📶\"", yamlQuote("Café 📶").c_str());
}

void test_scalar_round_trip() {
  const char* cases[] = {"", "x", "a\\b\"c\nd\te", "\x01", "Café 📶", "it's"};
  for (const char* c : cases) {
    std::string out, err;
    ScalarKind kind;
    TEST_ASSERT_TRUE_MESSAGE(yamlScalar(yamlQuote(c), out, kind, err), c);
    TEST_ASSERT_TRUE(kind == ScalarKind::Double);
    TEST_ASSERT_EQUAL_STRING(c, out.c_str());
  }
}

void test_scalar_plain_and_single() {
  expectOk("compact", "compact", ScalarKind::Plain);
  expectOk("  hello world  ", "hello world", ScalarKind::Plain);
  expectOk("'it''s'", "it's", ScalarKind::Single);
  expectOk("'a\\n'", "a\\n", ScalarKind::Single);
}

void test_scalar_comments_and_hash_in_quotes() {
  expectOk("\"Home\"  # main", "Home", ScalarKind::Double);
  expectOk("compact # small", "compact", ScalarKind::Plain);
  expectOk("\"a#b c#d\"", "a#b c#d", ScalarKind::Double);
  expectOk("'x # y'", "x # y", ScalarKind::Single);
  expectOk("a#b", "a#b", ScalarKind::Plain);
}

void test_scalar_errors() {
  const char* bad[] = {"\"open", "'open", "\"x\" junk", "\"\\q\"", "\"\\uD83D\""};
  for (const char* b : bad) {
    std::string out, err;
    ScalarKind kind;
    TEST_ASSERT_FALSE_MESSAGE(yamlScalar(b, out, kind, err), b);
    TEST_ASSERT_FALSE(err.empty());
  }
}


namespace {
Settings full() {
  Settings s;
  s.wifi = {{"Café \"📶\"\\", "p@ss\nword12"}, {"Phone", ""}};
  s.email = "me@example.com";
  s.refreshToken = "AMf-token";
  s.tz = "EST5EDT,M3.2.0,M11.1.0";
  s.tzName = "America/New_York";
  s.ntpServer = "time.example.org";
  s.pagePassword = "pagepw";
  s.apPassword = "appass123";
  s.textSize = TextSize::Comfortable;
  s.drawerOpen = true;
  s.buzzer = false;
  s.theme = ThemeId::WhiteBlack;
  s.rotation = 3;
  s.lastRoom = "random";
  return s;
}

const char* kHead = "format: monster-cyberspace-backup\nversion: 1\n";

bool run(const std::string& y, Settings& s, std::string& err, std::string& note) {
  return backupFromYaml(y, s, err, note);
}

void expectSame(const Settings& a, const Settings& b) {
  TEST_ASSERT_EQUAL(a.wifi.size(), b.wifi.size());
  for (size_t i = 0; i < a.wifi.size(); i++) {
    TEST_ASSERT_EQUAL_STRING(a.wifi[i].ssid.c_str(), b.wifi[i].ssid.c_str());
    TEST_ASSERT_EQUAL_STRING(a.wifi[i].password.c_str(), b.wifi[i].password.c_str());
  }
  TEST_ASSERT_EQUAL_STRING(a.email.c_str(), b.email.c_str());
  TEST_ASSERT_EQUAL_STRING(a.refreshToken.c_str(), b.refreshToken.c_str());
  TEST_ASSERT_EQUAL_STRING(a.tz.c_str(), b.tz.c_str());
  TEST_ASSERT_EQUAL_STRING(a.tzName.c_str(), b.tzName.c_str());
  TEST_ASSERT_EQUAL_STRING(a.ntpServer.c_str(), b.ntpServer.c_str());
  TEST_ASSERT_EQUAL_STRING(a.pagePassword.c_str(), b.pagePassword.c_str());
  TEST_ASSERT_EQUAL_STRING(a.apPassword.c_str(), b.apPassword.c_str());
  TEST_ASSERT_TRUE(a.textSize == b.textSize);
  TEST_ASSERT_EQUAL(a.drawerOpen, b.drawerOpen);
  TEST_ASSERT_EQUAL(a.buzzer, b.buzzer);
  TEST_ASSERT_TRUE(a.theme == b.theme);
  TEST_ASSERT_EQUAL(a.rotation, b.rotation);
  TEST_ASSERT_EQUAL_STRING(a.lastRoom.c_str(), b.lastRoom.c_str());
}

// Fails on y, leaves the settings as they were, and the error is want.
void expectFail(const std::string& y, const char* want) {
  Settings s = full(), before = full();
  std::string err, note;
  TEST_ASSERT_FALSE_MESSAGE(run(y, s, err, note), y.c_str());
  TEST_ASSERT_EQUAL_STRING(want, err.c_str());
  expectSame(before, s);
}
}  // namespace

void test_round_trip_every_field() {
  std::string y = backupToYaml(full(), {"M9", "0.1.0", 1793039400});
  Settings d;
  std::string err, note;
  TEST_ASSERT_TRUE_MESSAGE(run(y, d, err, note), err.c_str());
  expectSame(full(), d);
  TEST_ASSERT_EQUAL_STRING("", err.c_str());
  TEST_ASSERT_EQUAL_STRING("", note.c_str());
  TEST_ASSERT_NOT_NULL(strstr(y.c_str(), "created: \"2026-10-26T18:30:00Z\""));
  TEST_ASSERT_NOT_NULL(strstr(y.c_str(), "theme: wb  # default | bw | wb\nrotation: 3  # 0 landscape, 1 portrait, 2 landscape 180, 3 portrait 180\n"));
  TEST_ASSERT_EQUAL(0, y.rfind("# Monster Cyberspace settings backup.\n", 0));
}

void test_writer_no_clock_no_wifi() {
  Settings s = full();
  s.wifi.clear();
  std::string y = backupToYaml(s, {"M9", "0.1.0", 0});
  TEST_ASSERT_NOT_NULL(strstr(y.c_str(), "created: \"\"\n"));
  TEST_ASSERT_NOT_NULL(strstr(y.c_str(), "wifi: []\n"));
  Settings d = full();
  std::string err, note;
  TEST_ASSERT_TRUE_MESSAGE(run(y, d, err, note), err.c_str());
  TEST_ASSERT_EQUAL(0, d.wifi.size());
}

void test_hand_edits() {
  std::string y =
      "# my notes\n"
      "account:\n"
      "  email: me@x.org   # mine\n"
      "  refreshToken: 'tok''en'\n"
      "\n"
      "format: monster-cyberspace-backup\r\n"
      "version: 1\n"
      "wifi:\n"
      "  - ssid: Home\n"
      "    password: hunter22\n"
      "  - ssid: 'Cafe'\n"
      "    password: \"\"\n"
      "textSize: comfortable # big\n"
      "buzzer: false\n"
      "lastRoom: lobby\n";
  Settings s;
  std::string err, note;
  TEST_ASSERT_TRUE_MESSAGE(run(y, s, err, note), err.c_str());
  TEST_ASSERT_EQUAL_STRING("me@x.org", s.email.c_str());
  TEST_ASSERT_EQUAL_STRING("tok'en", s.refreshToken.c_str());
  TEST_ASSERT_EQUAL(2, s.wifi.size());
  TEST_ASSERT_EQUAL_STRING("hunter22", s.wifi[0].password.c_str());
  TEST_ASSERT_EQUAL_STRING("Cafe", s.wifi[1].ssid.c_str());
  TEST_ASSERT_TRUE(s.textSize == TextSize::Comfortable);
  TEST_ASSERT_FALSE(s.buzzer);
  TEST_ASSERT_TRUE(run(std::string(kHead) + "textSize: large\n", s, err, note));
  TEST_ASSERT_TRUE(s.textSize == TextSize::Large);
  TEST_ASSERT_NOT_EQUAL(std::string::npos, backupToYaml(s, {"M9", "0.1.0", 0}).find("textSize: large\n"));
  TEST_ASSERT_EQUAL_STRING("lobby", s.lastRoom.c_str());
}

void test_comment_only_value_is_empty() {
  Settings s = full();
  std::string err, note;
  TEST_ASSERT_TRUE_MESSAGE(
      run(std::string(kHead) + "pagePassword:   # none\nlastRoom: x\n", s, err, note), err.c_str());
  TEST_ASSERT_EQUAL_STRING("", s.pagePassword.c_str());
  TEST_ASSERT_EQUAL_STRING("x", s.lastRoom.c_str());
}

void test_nul_rejected() {
  expectFail(std::string(kHead) + "lastRoom: \"a\\u0000b\"\n", "line 3: value contains a NUL");
}

void test_bom_crlf_no_final_newline() {
  std::string y = "\xEF\xBB\xBF" "format: monster-cyberspace-backup\r\nversion: 1\r\nbuzzer: false";
  Settings s;
  std::string err, note;
  TEST_ASSERT_TRUE_MESSAGE(run(y, s, err, note), err.c_str());
  TEST_ASSERT_FALSE(s.buzzer);
}

void test_missing_keys_keep_current() {
  Settings s = full(), want = full();
  std::string err, note;
  TEST_ASSERT_TRUE_MESSAGE(run(std::string(kHead) + "buzzer: true\n", s, err, note), err.c_str());
  want.buzzer = true;
  expectSame(want, s);
  s = full();
  TEST_ASSERT_TRUE_MESSAGE(
      run(std::string(kHead) + "account:\n  email: \"n@x.org\"\n", s, err, note), err.c_str());
  want = full();
  want.email = "n@x.org";
  expectSame(want, s);
  s = full();
  TEST_ASSERT_TRUE_MESSAGE(run(std::string(kHead) + "account:\nbuzzer: true\n", s, err, note),
                           err.c_str());
  want = full();
  want.buzzer = true;
  expectSame(want, s);
}

void test_empty_wifi_list_clears() {
  Settings s = full();
  std::string err, note;
  TEST_ASSERT_TRUE(run(std::string(kHead) + "wifi: []\n", s, err, note));
  TEST_ASSERT_EQUAL(0, s.wifi.size());
}

void test_empty_token_replaces_current() {
  Settings s = full();
  std::string err, note;
  TEST_ASSERT_TRUE(run(std::string(kHead) + "account:\n  refreshToken: \"\"\n", s, err, note));
  TEST_ASSERT_EQUAL_STRING("", s.refreshToken.c_str());
  TEST_ASSERT_EQUAL_STRING("me@example.com", s.email.c_str());
}

void test_unknown_keys_ignored() {
  Settings s = full();
  std::string err, note;
  std::string y = std::string(kHead) +
                  "colour: red\nwifi:\n  - ssid: A\n    hidden: true\n    password: \"\"\n"
                  "account:\n  nick: x\n  email: e@x.org\n";
  TEST_ASSERT_TRUE_MESSAGE(run(y, s, err, note), err.c_str());
  TEST_ASSERT_EQUAL(1, s.wifi.size());
  TEST_ASSERT_EQUAL_STRING("e@x.org", s.email.c_str());
}

void test_unknown_nested_block_ignored() {
  Settings s = full();
  std::string err, note;
  std::string y = std::string(kHead) + "gps:\n  enabled: true\n  fix:\n    - a\nbuzzer: true\n";
  TEST_ASSERT_TRUE_MESSAGE(run(y, s, err, note), err.c_str());
  TEST_ASSERT_TRUE(s.buzzer);
}

void test_wrong_format_and_version() {
  Settings s = full();
  std::string err, note;
  TEST_ASSERT_FALSE(run("format: other\nversion: 1\n", s, err, note));
  TEST_ASSERT_EQUAL_STRING("Not a Monster Cyberspace backup", err.c_str());
  TEST_ASSERT_FALSE(run("version: 1\n", s, err, note));
  TEST_ASSERT_EQUAL_STRING("Not a Monster Cyberspace backup", err.c_str());
  TEST_ASSERT_FALSE(run("format: monster-cyberspace-backup\nversion: 2\n", s, err, note));
  TEST_ASSERT_EQUAL_STRING("Backup version 2 is newer than this firmware", err.c_str());
  TEST_ASSERT_FALSE(run("format: monster-cyberspace-backup\nversion: x\n", s, err, note));
  TEST_ASSERT_EQUAL_STRING("Not a Monster Cyberspace backup", err.c_str());
  TEST_ASSERT_FALSE(run("format: monster-cyberspace-backup\nversion: 0\n", s, err, note));
  TEST_ASSERT_EQUAL_STRING("Not a Monster Cyberspace backup", err.c_str());
  TEST_ASSERT_FALSE(run("format: monster-cyberspace-backup\n", s, err, note));
  TEST_ASSERT_EQUAL_STRING("Not a Monster Cyberspace backup", err.c_str());
  // Backups from the earlier names (Rhino, Platy Cyberspace) still restore.
  TEST_ASSERT_TRUE_MESSAGE(run("format: rhino-cyberspace-backup\nversion: 1\n", s, err, note),
                           err.c_str());
  TEST_ASSERT_TRUE_MESSAGE(run("format: platy-cyberspace-backup\nversion: 1\n", s, err, note),
                           err.c_str());
}

void test_bad_indent() {
  expectFail(std::string(kHead) + "wifi:\n  - ssid: A\n     password: \"\"\n", "line 5: bad indent");
  expectFail(std::string(kHead) +
                 "wifi:\n  - ssid: A\n    password: \"\"\n   - ssid: B\n     password: \"\"\n",
             "line 6: bad indent");
  expectFail(std::string(kHead) + "account:\n\temail: x\n", "line 4: bad indent");
  expectFail(std::string(kHead) + "  buzzer: true\n", "line 3: bad indent");
}

void test_expected_key_value() {
  expectFail(std::string(kHead) + "buzzer: true\njust text\n", "line 4: expected \"key: value\"");
  expectFail(std::string(kHead) + "wifi: [a]\n", "line 3: expected \"key: value\"");
}

void test_bad_values_rejected() {
  struct C { const char* body; const char* err; } cases[] = {
      {"wifi:\n  - ssid: A\n    password: short\n", "line 5: Wi-Fi password: 8-63 characters, or empty"},
      {"wifi:\n  - ssid: \"\"\n    password: \"\"\n", "line 4: network name: 1-32 bytes"},
      {"wifi:\n  - ssid: aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n    password: \"\"\n",
       "line 4: network name: 1-32 bytes"},
      {"apPassword: abc\n", "line 3: setup-AP password: 8-63 printable characters, or empty"},
      {"pagePassword: \"a\\nb\"\n", "line 3: page password: up to 63 printable characters"},
      {"textSize: huge\n", "line 3: text size: compact, comfortable or large"},
      {"theme: pink\n", "line 3: theme: default, bw or wb"},
      {"rotation: up\n", "line 3: orientation: 0 to 3"},
      {"buzzer: maybe\n", "line 3: expected true or false"},
      {"wifi:\n  - ssid: A\n    password: \"\"\n  - ssid: A\n    password: \"\"\n",
       "line 6: duplicate network \"A\""},
      {"lastRoom: \"\"\n", "line 3: last room: 1-63 printable characters"},
      {"timezone: \"\"\n", "line 3: time zone: 1-63 printable characters"},
      {"tzName: \"\\u0001\"\n", "line 3: time zone name: up to 63 printable characters"},
  };
  for (auto& c : cases) expectFail(std::string(kHead) + c.body, c.err);
}

void test_more_than_five_networks_truncated() {
  std::string y = kHead;
  y += "wifi:\n";
  for (int i = 0; i < 7; i++) y += "  - ssid: N" + std::to_string(i) + "\n    password: \"\"\n";
  Settings s;
  std::string err, note;
  TEST_ASSERT_TRUE_MESSAGE(run(y, s, err, note), err.c_str());
  TEST_ASSERT_EQUAL(5, s.wifi.size());
  TEST_ASSERT_EQUAL_STRING("N0", s.wifi[0].ssid.c_str());
  TEST_ASSERT_EQUAL_STRING("N4", s.wifi[4].ssid.c_str());
  TEST_ASSERT_EQUAL_STRING("Kept the first 5 of 7 networks.", note.c_str());
}

void test_backup_time_keys() {
  Settings s;
  std::string err, note;
  Settings p = full();
  p.tzName = "Europe/Paris";
  p.tz = "CET-1CEST,M3.5.0,M10.5.0/3";
  TEST_ASSERT_TRUE(run(backupToYaml(p, {"M9", "1", 0}), s, err, note));
  expectSame(p, s);
  p.tzName = "";
  p.tz = "XYZ3";
  TEST_ASSERT_TRUE(run(backupToYaml(p, {"M9", "1", 0}), s, err, note));
  TEST_ASSERT_EQUAL_STRING("", s.tzName.c_str());
  TEST_ASSERT_EQUAL_STRING("XYZ3", s.tz.c_str());

  s = full();
  TEST_ASSERT_TRUE(run(std::string(kHead) + "buzzer: true\n", s, err, note));
  TEST_ASSERT_EQUAL_STRING("America/New_York", s.tzName.c_str());
  TEST_ASSERT_EQUAL_STRING("time.example.org", s.ntpServer.c_str());

  TEST_ASSERT_TRUE(run(std::string(kHead) + "timezone: \"EST5EDT,M3.2.0,M11.1.0\"\n", s, err, note));
  TEST_ASSERT_EQUAL_STRING("America/New_York", s.tzName.c_str());
  TEST_ASSERT_TRUE(run(std::string(kHead) + "timezone: \"XYZ3\"\n", s, err, note));
  TEST_ASSERT_EQUAL_STRING("", s.tzName.c_str());

  expectFail(std::string(kHead) + "ntpServer: \"a b\"\n",
             "line 3: NTP server: a host name, 1-63 characters");
}

void test_backup_rotation() {
  Settings s = full();
  std::string err, note;
  TEST_ASSERT_TRUE(run(std::string(kHead) + "buzzer: true\n", s, err, note));
  TEST_ASSERT_EQUAL(3, s.rotation);  // missing: kept
  TEST_ASSERT_TRUE_MESSAGE(run(std::string(kHead) + "rotation: 1\n", s, err, note), err.c_str());
  TEST_ASSERT_EQUAL(1, s.rotation);
  for (const char* v : {"4", "-1", "255"}) {
    s.rotation = 2;
    TEST_ASSERT_TRUE_MESSAGE(run(std::string(kHead) + "rotation: " + v + "\n", s, err, note),
                             err.c_str());
    TEST_ASSERT_EQUAL(0, s.rotation);  // out of range: Landscape
  }
}

void test_duplicate_key() {
  expectFail(std::string(kHead) + "buzzer: true\nbuzzer: false\n", "line 4: duplicate key \"buzzer\"");
}

void test_oversize_refused() {
  expectFail(std::string(kMaxBackupBytes + 1, '#'), "Backup is over 16 KB");
}

void test_failure_leaves_settings_unchanged() {
  expectFail(std::string(kHead) + "wifi:\n  - ssid: A\n    password: \"\"\ntextSize: huge\n",
             "line 6: text size: compact, comfortable or large");
}

void test_file_name_with_clock() {
  std::vector<std::string> ex;
  TEST_ASSERT_EQUAL_STRING("rc-backup-20261026-183000.yaml", rc::backupFileName(1793039400, ex).c_str());
  ex.push_back("rc-backup-20261026-183000.yaml");
  TEST_ASSERT_EQUAL_STRING("rc-backup-20261026-183000-2.yaml", rc::backupFileName(1793039400, ex).c_str());
  ex.push_back("rc-backup-20261026-183000-2.yaml");
  TEST_ASSERT_EQUAL_STRING("rc-backup-20261026-183000-3.yaml", rc::backupFileName(1793039400, ex).c_str());
}

void test_file_name_without_clock() {
  TEST_ASSERT_EQUAL_STRING("rc-backup-1.yaml", rc::backupFileName(0, {}).c_str());
  TEST_ASSERT_EQUAL_STRING("rc-backup-8.yaml",
                           rc::backupFileName(0, {"rc-backup-1.yaml", "rc-backup-7.yaml", "x.yaml"}).c_str());
}

void test_valid_backup_name() {
  TEST_ASSERT_TRUE(rc::validBackupName("rc-backup-1.yaml"));
  TEST_ASSERT_TRUE(rc::validBackupName("My_file.v2.yaml"));
  for (const char* bad : {"../x.yaml", "a/b.yaml", ".yaml", "x.yml", "x.YAML", "a b.yaml", ""})
    TEST_ASSERT_FALSE(rc::validBackupName(bad));
  TEST_ASSERT_TRUE(rc::validBackupName(std::string(59, 'a') + ".yaml"));
  TEST_ASSERT_FALSE(rc::validBackupName(std::string(60, 'a') + ".yaml"));
}

void test_sort_backup_names() {
  std::vector<std::string> n = {"rc-backup-2.yaml", "old.yaml", "rc-backup-20261026-183000.yaml",
                                "rc-backup-10.yaml", "rc-backup-20261101-090000.yaml"};
  rc::sortBackupNames(n);
  std::vector<std::string> want = {"rc-backup-20261101-090000.yaml", "rc-backup-20261026-183000.yaml",
                                   "rc-backup-10.yaml", "rc-backup-2.yaml", "old.yaml"};
  TEST_ASSERT_TRUE(n == want);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_quote_escapes);
  RUN_TEST(test_scalar_round_trip);
  RUN_TEST(test_scalar_plain_and_single);
  RUN_TEST(test_scalar_comments_and_hash_in_quotes);
  RUN_TEST(test_scalar_errors);
  RUN_TEST(test_round_trip_every_field);
  RUN_TEST(test_writer_no_clock_no_wifi);
  RUN_TEST(test_hand_edits);
  RUN_TEST(test_comment_only_value_is_empty);
  RUN_TEST(test_nul_rejected);
  RUN_TEST(test_bom_crlf_no_final_newline);
  RUN_TEST(test_missing_keys_keep_current);
  RUN_TEST(test_empty_wifi_list_clears);
  RUN_TEST(test_empty_token_replaces_current);
  RUN_TEST(test_unknown_keys_ignored);
  RUN_TEST(test_unknown_nested_block_ignored);
  RUN_TEST(test_wrong_format_and_version);
  RUN_TEST(test_bad_indent);
  RUN_TEST(test_expected_key_value);
  RUN_TEST(test_bad_values_rejected);
  RUN_TEST(test_more_than_five_networks_truncated);
  RUN_TEST(test_backup_time_keys);
  RUN_TEST(test_backup_rotation);
  RUN_TEST(test_duplicate_key);
  RUN_TEST(test_oversize_refused);
  RUN_TEST(test_failure_leaves_settings_unchanged);
  RUN_TEST(test_file_name_with_clock);
  RUN_TEST(test_file_name_without_clock);
  RUN_TEST(test_valid_backup_name);
  RUN_TEST(test_sort_backup_names);
  return UNITY_END();
}
