#pragma once

#include <string>

// Portable pieces of the over-the-air update: where releases live and how
// versions compare. The device side is src/app/ota_service.cpp.
namespace rc {

// GitHub answers this with a redirect to /releases/tag/<tag> of the newest
// release, so the tag comes from the Location header (no API, no JSON).
constexpr const char* kOtaLatestUrl = "https://github.com/oumike/monster-cyberspace/releases/latest";

// Numeric components before any '-' or '+' ("v0.2.10+abc" -> 0,2,10); missing
// components count as 0. Build metadata never makes a build look newer.
// Returns -1, 0 or 1.
int compareVersions(const char* a, const char* b);

// "v<major>.<minor>.<patch>" from a .../releases/tag/<tag> Location, else "".
std::string tagFromLocation(const std::string& location);

// monster-cyberspace-<slug>-<tag>-ota.bin, as scripts/release.sh names it.
std::string otaAssetName(const std::string& slug, const std::string& tag);
std::string otaAssetUrl(const std::string& slug, const std::string& tag);

}  // namespace rc
