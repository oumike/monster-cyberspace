#pragma once

// Compile-time feature flags. Each defaults to 1; a board env turns one off
// with -DRC_FEAT_X=0. A feature's own .cpp files wrap their body in
// #if RC_FEAT_X; everything else guards only its wiring points.

// cIRC rooms: Home's Messages and Rooms, the room keys, the room list and
// chat-mention alerts. C-Mail keeps RoomScreen, so it is built either way.
#ifndef RC_FEAT_CIRC
#define RC_FEAT_CIRC 1
#endif
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
// Notes (private notes with revisions): PSRAM boards only, with the Feed,
// whose worker runs them.
#ifndef RC_FEAT_NOTES
#define RC_FEAT_NOTES RC_FEAT_FEED
#endif
// Programs (the read-only gallery): with the Feed, whose worker runs it, and
// PSRAM for source up to the HTTPS client's cap.
#ifndef RC_FEAT_PROGRAMS
#if RC_LOW_MEM
#define RC_FEAT_PROGRAMS 0
#else
#define RC_FEAT_PROGRAMS RC_FEAT_FEED
#endif
#endif
#ifndef RC_FEAT_MASCOT
#define RC_FEAT_MASCOT 1
#endif

// Shared by several features: the entry lists, the Entry screen and the
// network side that loads them serve the Feed, guild threads and a user's
// posts; the Composer writes Feed posts and guild threads.
#define RC_FEAT_ENTRIES (RC_FEAT_FEED || RC_FEAT_GUILDS || RC_FEAT_PEOPLE)
#define RC_FEAT_COMPOSER (RC_FEAT_FEED || RC_FEAT_GUILDS)

// Search (people, entries, replies): its hits open profiles and entries, and
// the entries worker runs it; PSRAM boards only.
#ifndef RC_FEAT_SEARCH
#if RC_LOW_MEM
#define RC_FEAT_SEARCH 0
#else
#define RC_FEAT_SEARCH (RC_FEAT_PEOPLE && RC_FEAT_ENTRIES)
#endif
#endif

namespace rc {

// The features that shape portable choices (Home's tiles, Settings items).
struct Features {
  bool cmail, feed, guilds, people, unread;
  bool circ = true;
  bool notes = true;
  bool programs = true;
  bool search = true;
};

constexpr Features kBuiltFeatures{RC_FEAT_CMAIL != 0, RC_FEAT_FEED != 0, RC_FEAT_GUILDS != 0,
                                  RC_FEAT_PEOPLE != 0, RC_FEAT_UNREAD != 0, RC_FEAT_CIRC != 0,
                                  RC_FEAT_NOTES != 0, RC_FEAT_PROGRAMS != 0,
                                  RC_FEAT_SEARCH != 0};

}  // namespace rc
