#include "tz/zones.h"

namespace rc {

namespace {

// POSIX strings are the TZif footers of tzdata 2026e (`tail -n1 /usr/share/zoneinfo/<zone>`).
// Sorted by IANA name.
constexpr Zone kZones[] = {
    {"Africa/Cairo", "Egypt (Cairo)", "EET-2EEST,M4.5.5/0,M10.5.4/24"},
    {"Africa/Johannesburg", "South Africa", "SAST-2"},
    {"Africa/Lagos", "Nigeria (Lagos)", "WAT-1"},
    {"Africa/Nairobi", "Kenya (Nairobi)", "EAT-3"},
    {"America/Anchorage", "US Alaska", "AKST9AKDT,M3.2.0,M11.1.0"},
    {"America/Argentina/Buenos_Aires", "Argentina", "<-03>3"},
    {"America/Bogota", "Colombia (Bogota)", "<-05>5"},
    {"America/Caracas", "Venezuela (Caracas)", "<-04>4"},
    {"America/Chicago", "US Central (Chicago)", "CST6CDT,M3.2.0,M11.1.0"},
    {"America/Denver", "US Mountain (Denver)", "MST7MDT,M3.2.0,M11.1.0"},
    {"America/Halifax", "Canada Atlantic", "AST4ADT,M3.2.0,M11.1.0"},
    {"America/Los_Angeles", "US Pacific (LA)", "PST8PDT,M3.2.0,M11.1.0"},
    {"America/Mexico_City", "Mexico City", "CST6"},
    {"America/New_York", "US Eastern (New York)", "EST5EDT,M3.2.0,M11.1.0"},
    {"America/Noronha", "Brazil (Noronha)", "<-02>2"},
    {"America/Phoenix", "US Arizona (Phoenix)", "MST7"},
    {"America/Santiago", "Chile (Santiago)", "<-04>4<-03>,M9.1.6/24,M4.1.6/24"},
    {"America/Sao_Paulo", "Brazil (Sao Paulo)", "<-03>3"},
    {"America/St_Johns", "Canada Newfoundland", "NST3:30NDT,M3.2.0,M11.1.0"},
    {"America/Toronto", "Canada Eastern", "EST5EDT,M3.2.0,M11.1.0"},
    {"Asia/Bangkok", "Thailand (Bangkok)", "<+07>-7"},
    {"Asia/Dhaka", "Bangladesh (Dhaka)", "<+06>-6"},
    {"Asia/Dubai", "Gulf (Dubai)", "<+04>-4"},
    {"Asia/Ho_Chi_Minh", "Vietnam", "<+07>-7"},
    {"Asia/Hong_Kong", "Hong Kong", "HKT-8"},
    {"Asia/Jakarta", "Indonesia (Jakarta)", "WIB-7"},
    {"Asia/Jerusalem", "Israel", "IST-2IDT,M3.4.4/26,M10.5.0"},
    {"Asia/Kabul", "Afghanistan", "<+0430>-4:30"},
    {"Asia/Karachi", "Pakistan", "PKT-5"},
    {"Asia/Kathmandu", "Nepal", "<+0545>-5:45"},
    {"Asia/Kolkata", "India", "IST-5:30"},
    {"Asia/Manila", "Philippines", "PST-8"},
    {"Asia/Riyadh", "Saudi Arabia", "<+03>-3"},
    {"Asia/Seoul", "Korea (Seoul)", "KST-9"},
    {"Asia/Shanghai", "China", "CST-8"},
    {"Asia/Singapore", "Singapore", "<+08>-8"},
    {"Asia/Tehran", "Iran", "<+0330>-3:30"},
    {"Asia/Tokyo", "Japan", "JST-9"},
    {"Asia/Yangon", "Myanmar", "<+0630>-6:30"},
    {"Atlantic/Azores", "Azores", "<-01>1<+00>,M3.5.0/0,M10.5.0/1"},
    {"Atlantic/Cape_Verde", "Cape Verde", "<-01>1"},
    {"Australia/Adelaide", "Australia Adelaide", "ACST-9:30ACDT,M10.1.0,M4.1.0/3"},
    {"Australia/Brisbane", "Australia Brisbane", "AEST-10"},
    {"Australia/Darwin", "Australia Darwin", "ACST-9:30"},
    {"Australia/Eucla", "Australia Eucla", "<+0845>-8:45"},
    {"Australia/Lord_Howe", "Lord Howe Island", "<+1030>-10:30<+11>-11,M10.1.0,M4.1.0"},
    {"Australia/Perth", "Australia Perth", "AWST-8"},
    {"Australia/Sydney", "Australia Sydney", "AEST-10AEDT,M10.1.0,M4.1.0/3"},
    {"Etc/UTC", "UTC", "UTC0"},
    {"Europe/Athens", "Greece (Athens)", "EET-2EEST,M3.5.0/3,M10.5.0/4"},
    {"Europe/Berlin", "Germany (Berlin)", "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Europe/Dublin", "Ireland (Dublin)", "GMT0IST,M3.5.0/1,M10.5.0"},
    {"Europe/Helsinki", "Finland (Helsinki)", "EET-2EEST,M3.5.0/3,M10.5.0/4"},
    {"Europe/Istanbul", "Turkey (Istanbul)", "<+03>-3"},
    {"Europe/Kyiv", "Ukraine (Kyiv)", "EET-2EEST,M3.5.0/3,M10.5.0/4"},
    {"Europe/Lisbon", "Portugal (Lisbon)", "WET0WEST,M3.5.0/1,M10.5.0"},
    {"Europe/London", "UK (London)", "GMT0BST,M3.5.0/1,M10.5.0"},
    {"Europe/Madrid", "Spain (Madrid)", "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Europe/Moscow", "Russia (Moscow)", "MSK-3"},
    {"Europe/Paris", "France (Paris)", "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Europe/Rome", "Italy (Rome)", "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Europe/Warsaw", "Poland (Warsaw)", "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Pacific/Auckland", "New Zealand", "NZST-12NZDT,M9.5.0,M4.1.0/3"},
    {"Pacific/Chatham", "Chatham Islands", "<+1245>-12:45<+1345>,M9.5.0/2:45,M4.1.0/3:45"},
    {"Pacific/Fiji", "Fiji", "<+12>-12"},
    {"Pacific/Honolulu", "US Hawaii", "HST10"},
    {"Pacific/Kiritimati", "Kiribati (Line Is.)", "<+14>-14"},
    {"Pacific/Marquesas", "Marquesas Islands", "<-0930>9:30"},
    {"Pacific/Noumea", "New Caledonia", "<+11>-11"},
    {"Pacific/Pago_Pago", "American Samoa", "SST11"},
    {"Pacific/Tongatapu", "Tonga", "<+13>-13"},
};

}  // namespace

size_t zoneCount() { return sizeof kZones / sizeof kZones[0]; }

const Zone& zoneAt(size_t i) { return kZones[i]; }

const Zone* findZoneByIana(std::string_view iana) {
  for (const Zone& z : kZones)
    if (iana == z.iana) return &z;
  return nullptr;
}

const Zone* findZoneByPosix(std::string_view posix) {
  for (const Zone& z : kZones)
    if (posix == z.posix) return &z;
  return nullptr;
}

const char* zoneLabel(std::string_view iana) {
  const Zone* z = findZoneByIana(iana);
  return z ? z->label : "Custom";
}

bool validPosixTz(std::string_view tz) {
  if (tz.empty() || tz.size() > 63) return false;
  for (char c : tz)
    if (c < 0x20 || c > 0x7E) return false;
  return true;
}

}  // namespace rc
