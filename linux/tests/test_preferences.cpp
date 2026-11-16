// SPDX-License-Identifier: GPL-3.0-or-later
// The Preferences shim: NVS-like namespaces in files under MONSTER_CONFIG_DIR.
#include <Preferences.h>
#include <sys/stat.h>

#include <fstream>

#include "check.h"

namespace {

std::string dir;

mode_t modeOf(const std::string& p) {
  struct stat st {};
  if (stat(p.c_str(), &st) != 0) return 0;
  return st.st_mode & 0777;
}

void nothingSavedYet() {
  Preferences p;
  CHECK(!p.begin("rcfg", true));  // read-only on a missing namespace fails, as NVS does
  CHECK(p.begin("rcfg", false));  // read-write creates it
  p.end();
}

void everyTypeRoundTrips() {
  Preferences p;
  CHECK(p.begin("rcfg", false));
  CHECK(p.putBool("b", true) > 0);
  CHECK(p.putUChar("u8", 200) > 0);
  CHECK(p.putUShort("u16", 60000) > 0);
  CHECK(p.putUInt("u32", 4000000000u) > 0);
  CHECK(p.putString("s", "tab\there\nnew line \xE2\x9C\x93") > 0);
  CHECK(p.putString("empty", "") >= 0);
  CHECK(p.getBool("b", false));
  CHECK_EQ(p.getUChar("u8", 0), 200);
  CHECK_EQ(p.getUShort("u16", 0), 60000);
  CHECK_EQ(p.getUInt("u32", 0), 4000000000u);
  CHECK(std::string(p.getString("s", "x").c_str()) == "tab\there\nnew line \xE2\x9C\x93");
  CHECK(p.isKey("empty"));
  CHECK(std::string(p.getString("empty", "x").c_str()).empty());
  // Bool and UChar share NVS's u8; other types don't mix.
  CHECK_EQ(p.getUChar("b", 7), 1);
  CHECK_EQ(p.getUShort("u8", 7), 7);
  CHECK(std::string(p.getString("u32", "def").c_str()) == "def");
  // Missing keys give the default.
  CHECK_EQ(p.getUInt("nope", 42), 42u);
  CHECK(!p.isKey("nope"));
  p.end();
}

void keysLongerThan15Fail() {
  Preferences p;
  CHECK(p.begin("rcfg", false));
  CHECK_EQ(p.putUChar("sixteen_chars_xx", 1), 0u);
  CHECK(p.putUChar("fifteen_chars_x", 1) > 0);
  p.end();
}

void readOnlyRefusesWrites() {
  Preferences p;
  CHECK(p.begin("rcfg", true));
  CHECK_EQ(p.putUChar("u8", 1), 0u);
  CHECK(!p.remove("u8"));
  CHECK_EQ(p.getUChar("u8", 0), 200);
  p.end();
}

void persistsOnDisk() {
  compat::preferencesForget();  // as a fresh process would
  Preferences p;
  CHECK(p.begin("rcfg", true));
  CHECK_EQ(p.getUInt("u32", 0), 4000000000u);
  CHECK(std::string(p.getString("s", "").c_str()) == "tab\there\nnew line \xE2\x9C\x93");
  p.end();
  CHECK_EQ(modeOf(dir), mode_t(0700));
  CHECK_EQ(modeOf(dir + "/rcfg"), mode_t(0600));
}

void leftoverTmpIsIgnored() {
  {
    std::ofstream junk(dir + "/rcfg.tmp");
    junk << "garbage from a write cut short\n";
  }
  compat::preferencesForget();
  Preferences p;
  CHECK(p.begin("rcfg", true));
  CHECK_EQ(p.getUChar("u8", 0), 200);
  p.end();
}

void removeAndClear() {
  Preferences p;
  CHECK(p.begin("rcfg", false));
  CHECK(p.remove("u8"));
  CHECK(!p.isKey("u8"));
  CHECK(!p.remove("u8"));
  CHECK(p.clear());
  CHECK(!p.isKey("s"));
  p.end();
  compat::preferencesForget();
  CHECK(p.begin("rcfg", true));
  CHECK(!p.isKey("u16"));
  p.end();
}

void namespacesAreSeparate() {
  Preferences a, b;
  CHECK(a.begin("rcfg", false));
  CHECK(b.begin("rcread", false));
  CHECK(a.putUChar("k", 1) > 0);
  CHECK(b.putUChar("k", 2) > 0);
  CHECK_EQ(a.getUChar("k", 0), 1);
  CHECK_EQ(b.getUChar("k", 0), 2);
  a.end();
  b.end();
}

}  // namespace

int main() {
  dir = tempDir() + "/cfg";
  setenv("MONSTER_CONFIG_DIR", dir.c_str(), 1);
  nothingSavedYet();
  everyTypeRoundTrips();
  keysLongerThan15Fail();
  readOnlyRefusesWrites();
  persistsOnDisk();
  leftoverTmpIsIgnored();
  removeAndClear();
  namespacesAreSeparate();
  return checkResult();
}
