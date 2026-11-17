// SPDX-License-Identifier: GPL-3.0-or-later
// The FS shim: Arduino's fs::FS and File over POSIX files under a root.
#include <FS.h>
#include <sys/stat.h>

#include <set>
#include <string>

#include "check.h"

int main() {
  const std::string root = tempDir();
  fs::FS fs(root);

  CHECK(!fs.exists("/backups"));
  CHECK(fs.mkdir("/backups"));
  CHECK(fs.exists("/backups"));
  CHECK(!fs.open("/backups/missing.yaml", "r"));

  {
    File f = fs.open("/backups/a.yaml", "w");
    CHECK(f);
    CHECK(!f.isDirectory());
    const std::string text = "theme: 3\n";
    CHECK_EQ(f.write(reinterpret_cast<const uint8_t*>(text.data()), text.size()), text.size());
    f.close();
    CHECK(!f);
  }
  {
    File f = fs.open("/backups/b.yaml", "w");
    f.write(reinterpret_cast<const uint8_t*>("x"), 1);
  }  // closed by the destructor
  CHECK(fs.mkdir("/backups/sub"));

  {
    File f = fs.open("/backups/a.yaml", "r");
    CHECK(f);
    CHECK_EQ(f.size(), size_t(9));
    std::string got(9, '\0');
    CHECK_EQ(f.read(reinterpret_cast<uint8_t*>(&got[0]), got.size()), size_t(9));
    CHECK(got == "theme: 3\n");
  }

  File dir = fs.open("/backups");
  CHECK(dir);
  CHECK(dir.isDirectory());
  std::set<std::string> files, dirs;
  for (File e = dir.openNextFile(); e; e = dir.openNextFile()) {
    (e.isDirectory() ? dirs : files).insert(e.name());
    if (!e.isDirectory()) CHECK(e.size() > 0);
  }
  dir.close();
  CHECK(files == (std::set<std::string>{"a.yaml", "b.yaml"}));
  CHECK(dirs == (std::set<std::string>{"sub"}));

  CHECK(fs.remove("/backups/b.yaml"));
  CHECK(!fs.exists("/backups/b.yaml"));
  CHECK(!fs.remove("/backups/b.yaml"));
  // Paths can't climb out of the root.
  CHECK(!fs.open("/../escape", "w"));
  // Files are the owner's only: backups hold passwords.
  struct stat st {};
  CHECK(::stat((root + "/backups/a.yaml").c_str(), &st) == 0);
  CHECK_EQ(int(st.st_mode & 0777), 0600);

  // Mounted at a path: the app's "/monster-cyberspace/backups" is the
  // root's "backups", and nothing outside the mount path is reachable.
  const std::string data = tempDir();
  fs::FS mounted(data, "/monster-cyberspace");
  CHECK(mounted.mkdir("/monster-cyberspace/backups"));
  CHECK(::stat((data + "/backups").c_str(), &st) == 0 && S_ISDIR(st.st_mode));
  {
    File f = mounted.open("/monster-cyberspace/backups/c.yaml", "w");
    CHECK(f);
  }
  CHECK(::stat((data + "/backups/c.yaml").c_str(), &st) == 0);
  CHECK(mounted.exists("/monster-cyberspace"));
  CHECK(!mounted.exists("/other"));
  CHECK(!mounted.open("/monster-cyberspaceX/a", "w"));
  CHECK(!mounted.mkdir("/backups"));
  return checkResult();
}
