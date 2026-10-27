#include "store/backup_store.h"

#include <FS.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <algorithm>
#include <ctime>
#include <map>

#include "app/restart.h"
#include "config/backup_yaml.h"
#include "core_version.h"
#include "hal/board.h"
#include "store/settings.h"

namespace backupstore {

namespace {

constexpr const char* kRoot = "/platy-cyberspace";
constexpr const char* kDir = "/platy-cyberspace/backups";

SemaphoreHandle_t gMutex = nullptr;

struct Lock {
  Lock() {
    if (gMutex) xSemaphoreTake(gMutex, portMAX_DELAY);
  }
  ~Lock() {
    if (gMutex) xSemaphoreGive(gMutex);
  }
};

// Mounts the card on construction; unmounts on every return path. fs is null
// when there is no slot or no card.
struct SdGuard {
  fs::FS* fs = nullptr;
  SdGuard() { fs = board::sdMount(); }
  ~SdGuard() {
    if (fs) board::sdUnmount();
  }
  SdGuard(const SdGuard&) = delete;
  SdGuard& operator=(const SdGuard&) = delete;
};

void wipe(std::string& s) {
  std::fill(s.begin(), s.end(), '\0');
  s.clear();
}

void wipeSecrets(rc::Settings& s) {
  wipe(s.refreshToken);
  wipe(s.pagePassword);
  wipe(s.apPassword);
  for (auto& w : s.wifi) wipe(w.password);
}

std::string baseName(const char* p) {
  std::string s(p ? p : "");
  size_t slash = s.find_last_of('/');
  return slash == std::string::npos ? s : s.substr(slash + 1);
}

rc::BackupMeta meta(int64_t epoch) {
  rc::BackupMeta m;
  m.board = board::name();
  m.firmware = RC_VERSION;
  m.epochSec = epoch;
  return m;
}

// Valid backup names already on the card, for backupFileName's N counter.
void collectNames(fs::FS& fs, std::vector<std::string>& names) {
  File dir = fs.open(kDir);
  if (dir && dir.isDirectory()) {
    for (File e = dir.openNextFile(); e; e = dir.openNextFile()) {
      std::string n = baseName(e.name());
      bool isDir = e.isDirectory();
      e.close();
      if (!isDir && rc::validBackupName(n)) names.push_back(n);
    }
  }
  if (dir) dir.close();
}

}  // namespace

void begin() {
  if (!gMutex) gMutex = xSemaphoreCreateMutex();
}

void currentYaml(std::string& yaml, std::string& fileName) {
  Lock lock;
  rc::Settings s;
  settings::load(s);
  int64_t epoch = static_cast<int64_t>(time(nullptr));
  yaml = rc::backupToYaml(s, meta(epoch));
  std::vector<std::string> none;
  fileName = rc::backupFileName(epoch, none);
  wipeSecrets(s);
}

Result saveToSd() {
  Lock lock;
  SdGuard sd;
  if (!sd.fs) return {false, "No SD card"};

  std::vector<std::string> existing;
  collectNames(*sd.fs, existing);

  rc::Settings s;
  settings::load(s);
  int64_t epoch = static_cast<int64_t>(time(nullptr));
  std::string yaml = rc::backupToYaml(s, meta(epoch));
  wipeSecrets(s);

  std::string name = rc::backupFileName(epoch, existing);

  if (!sd.fs->exists(kRoot)) sd.fs->mkdir(kRoot);
  if (!sd.fs->exists(kDir)) sd.fs->mkdir(kDir);

  std::string path = std::string(kDir) + "/" + name;
  File f = sd.fs->open(path.c_str(), "w");
  if (!f) {
    wipe(yaml);
    return {false, "SD write failed"};
  }
  size_t wrote = f.write(reinterpret_cast<const uint8_t*>(yaml.data()), yaml.size());
  f.close();
  if (wrote != yaml.size()) {
    sd.fs->remove(path.c_str());
    wipe(yaml);
    return {false, "SD write failed"};
  }
  wipe(yaml);
  return {true, "Saved " + name};
}

Result listSd(std::vector<FileInfo>& out) {
  Lock lock;
  SdGuard sd;
  if (!sd.fs) return {false, "No SD card"};

  out.clear();
  std::map<std::string, size_t> sizes;
  std::vector<std::string> names;
  File dir = sd.fs->open(kDir);
  if (dir && dir.isDirectory()) {
    for (File e = dir.openNextFile(); e; e = dir.openNextFile()) {
      std::string n = baseName(e.name());
      bool isDir = e.isDirectory();
      size_t sz = e.size();
      e.close();
      if (!isDir && rc::validBackupName(n)) {
        names.push_back(n);
        sizes[n] = sz;
      }
    }
  }
  if (dir) dir.close();

  rc::sortBackupNames(names);
  for (auto& n : names) out.push_back({n, sizes[n]});
  return {true, ""};
}

Result readSd(const std::string& name, std::string& yaml) {
  Lock lock;
  if (!rc::validBackupName(name)) return {false, "Bad file name"};
  SdGuard sd;
  if (!sd.fs) return {false, "No SD card"};

  std::string path = std::string(kDir) + "/" + name;
  File f = sd.fs->open(path.c_str(), "r");
  if (!f) return {false, "SD read failed"};
  size_t sz = f.size();
  if (sz > rc::kMaxBackupBytes) {
    f.close();
    return {false, "Restore failed: Backup is over 16 KB"};
  }
  yaml.resize(sz);
  size_t got = sz ? f.read(reinterpret_cast<uint8_t*>(&yaml[0]), sz) : 0;
  f.close();
  if (got != sz) {
    wipe(yaml);
    return {false, "SD read failed"};
  }
  return {true, ""};
}

Result deleteSd(const std::string& name) {
  Lock lock;
  if (!rc::validBackupName(name)) return {false, "Bad file name"};
  SdGuard sd;
  if (!sd.fs) return {false, "No SD card"};

  std::string path = std::string(kDir) + "/" + name;
  if (!sd.fs->remove(path.c_str())) return {false, "SD delete failed"};
  return {true, "Deleted " + name};
}

Result applyAndRestart(std::string_view yaml) {
  Lock lock;
  rc::Settings s;
  settings::load(s);
  std::string err, note;
  bool ok = rc::backupFromYaml(yaml, s, err, note);
  if (!ok) {
    wipeSecrets(s);
    return {false, "Restore failed: " + err};
  }
  settings::saveAll(s);
  wipeSecrets(s);
  rc::restartSoon(1000);
  std::string msg = "Restored. ";
  if (!note.empty()) msg += note + " ";
  msg += "Restarting\u2026";
  return {true, msg};
}

}  // namespace backupstore
