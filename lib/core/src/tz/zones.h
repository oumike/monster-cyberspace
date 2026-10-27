#pragma once

#include <cstddef>
#include <string_view>

// The time zones offered on the setup page and the device's Settings: every
// inhabited UTC offset at least once, plus the larger population centres.
namespace rc {

struct Zone {
  const char* iana;   // e.g. "America/New_York"
  const char* label;  // short, for the device menu
  const char* posix;  // the TZ string newlib applies
};

constexpr size_t kMaxZoneLabel = 22;

size_t zoneCount();
const Zone& zoneAt(size_t i);
const Zone* findZoneByIana(std::string_view iana);
// The first zone with this rule; several zones can share one.
const Zone* findZoneByPosix(std::string_view posix);
// The zone's label, or "Custom" for a name not in the list (empty included).
const char* zoneLabel(std::string_view iana);

// A POSIX TZ string as the web config accepts it: 1-63 printable ASCII.
bool validPosixTz(std::string_view tz);

}  // namespace rc
