#pragma once

// Compile-time feature flags. Each defaults to 1; a board env turns one off
// with -DRC_FEAT_X=0. A feature's own .cpp files wrap their body in
// #if RC_FEAT_X; everything else guards only its wiring points.

#ifndef RC_FEAT_CMAIL
#define RC_FEAT_CMAIL 1
#endif
#ifndef RC_FEAT_FEED
#define RC_FEAT_FEED 1
#endif
#ifndef RC_FEAT_GUILDS
#define RC_FEAT_GUILDS 1
#endif
#ifndef RC_FEAT_PEOPLE
#define RC_FEAT_PEOPLE 1
#endif
#ifndef RC_FEAT_UNREAD
#define RC_FEAT_UNREAD 1
#endif
#ifndef RC_FEAT_SPELL
#define RC_FEAT_SPELL 1
#endif
#ifndef RC_FEAT_EMOJI
#define RC_FEAT_EMOJI 1
#endif
#ifndef RC_FEAT_OTA
#define RC_FEAT_OTA 1
#endif
#ifndef RC_FEAT_MASCOT
#define RC_FEAT_MASCOT 1
#endif

namespace rc {

// The features that shape portable choices (Home's tiles, Settings items).
struct Features {
  bool cmail, feed, guilds, people, unread;
};

constexpr Features kBuiltFeatures{RC_FEAT_CMAIL != 0, RC_FEAT_FEED != 0, RC_FEAT_GUILDS != 0,
                                  RC_FEAT_PEOPLE != 0, RC_FEAT_UNREAD != 0};

}  // namespace rc
