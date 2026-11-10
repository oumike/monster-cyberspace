#pragma once

#include <string>

#include "people/profile.h"

namespace rc {

// The fields Edit profile changes (PATCH /v1/users/me).
struct ProfileFields {
  std::string displayName, bio, websiteUrl, websiteName, locationName;
};

ProfileFields profileFields(const Profile& p);

// The PATCH body: only the fields that differ; one that is empty after
// trimming spaces is sent as null (cleared). "" when nothing changed.
std::string profilePatchJson(const ProfileFields& before, const ProfileFields& after);

// The first field over its limit (counted in characters), or a website
// without http(s)://; nullptr when all are fine.
const char* profileFieldError(const ProfileFields& f);

}  // namespace rc
