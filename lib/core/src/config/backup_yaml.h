#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "config/settings_model.h"

namespace rc {

constexpr size_t kMaxBackupBytes = 16384;

struct BackupMeta {
  std::string board, firmware;  // board::name(), RC_VERSION
  int64_t epochSec = 0;         // < 1700000000 means the clock isn't set
};

std::string backupToYaml(const Settings& s, const BackupMeta& m);

// Parses, validates, then merges into inOut. On false, err holds the first
// error and inOut is untouched. note is "" or "Kept the first 5 of N networks."
bool backupFromYaml(std::string_view yaml, Settings& inOut, std::string& err, std::string& note);

// epochSec >= 1700000000: "rc-backup-YYYYMMDD-HHMMSS.yaml" (UTC); on a clash
// "...-HHMMSS-2.yaml", "-3", ... Without a clock: "rc-backup-N.yaml", N one more
// than the highest such N in existing (1 when none).
std::string backupFileName(int64_t epochSec, const std::vector<std::string>& existing);

// 1-64 bytes of [A-Za-z0-9._-], ending in ".yaml", with a non-empty stem.
bool validBackupName(std::string_view name);

// Newest first: dated names descending, then rc-backup-N descending by N,
// then any other names in ascending byte order.
void sortBackupNames(std::vector<std::string>& names);

}  // namespace rc
