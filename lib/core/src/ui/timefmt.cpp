#include "ui/timefmt.h"

#include <cstdio>
#include <ctime>

namespace rc {

static std::tm localTm(int64_t epochMs) {
  std::time_t t = static_cast<std::time_t>(epochMs / 1000);
  std::tm tm{};
  localtime_r(&t, &tm);
  return tm;
}

std::string formatHHMM(int64_t epochMs) {
  std::tm tm = localTm(epochMs);
  char buf[8];
  std::snprintf(buf, sizeof buf, "%02d:%02d", tm.tm_hour, tm.tm_min);
  return buf;
}

std::string formatDay(int64_t epochMs) {
  static const char* const kDays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  static const char* const kMonths[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  std::tm tm = localTm(epochMs);
  char buf[16];
  std::snprintf(buf, sizeof buf, "%s %d %s", kDays[tm.tm_wday % 7], tm.tm_mday,
                kMonths[tm.tm_mon % 12]);
  return buf;
}

int64_t localDayNumber(int64_t epochMs) {
  std::tm tm = localTm(epochMs);
  return static_cast<int64_t>(tm.tm_year) * 400 + tm.tm_yday;
}

int64_t localMinuteNumber(int64_t epochMs) {
  std::tm tm = localTm(epochMs);
  return (localDayNumber(epochMs) * 24 + tm.tm_hour) * 60 + tm.tm_min;
}

}  // namespace rc
