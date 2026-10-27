#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

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

}  // namespace rc
