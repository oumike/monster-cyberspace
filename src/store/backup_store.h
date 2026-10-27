#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

// Reads and writes settings backups on the removable SD card and applies a
// restore. Every call mounts the card, works, then unmounts it; one operation
// runs at a time (a mutex). Backups carry secrets, so YAML is never logged and
// local copies are wiped after use.
namespace backupstore {

struct Result {
  bool ok = false;
  std::string message;  // user-facing copy
};

struct FileInfo {
  std::string name;
  size_t size = 0;
};

// Creates the mutex. Call once, after board::begin().
void begin();

// The current settings (fresh from NVS, token included) as YAML, plus a
// download file name. The caller wipes yaml after sending it. No SD access.
void currentYaml(std::string& yaml, std::string& fileName);

Result saveToSd();                                          // "Saved <name>"
Result listSd(std::vector<FileInfo>& out);                 // valid names only, newest first
Result readSd(const std::string& name, std::string& yaml); // refuses > kMaxBackupBytes
Result deleteSd(const std::string& name);                  // "Deleted <name>"

// Merges yaml into the NVS settings, saves them, then restartSoon(1000):
// "Restored. Restarting\u2026", with the note before "Restarting\u2026" when
// set. A failure is "Restore failed: <err>" and saves nothing.
Result applyAndRestart(std::string_view yaml);

}  // namespace backupstore
