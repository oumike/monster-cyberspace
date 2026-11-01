#include "screens/router.h"

#include "app/app_state.h"
#include "features.h"
#include "screens/composer.h"
#include "screens/entry.h"
#include "screens/room.h"
#include "screens/screens.h"

namespace rc::router {

namespace {

SplashScreen splash;
SetupScreen setup;
HomeScreen home;
RoomListScreen roomList;
RoomScreen room;
SettingsScreen settingsScreen;
ComingSoonScreen comingSoon;
LoginFormScreen loginForm;
WifiScreen wifiScreen;
// Each feature's screens exist only when it is built (features.h).
#if RC_FEAT_CMAIL
CmailListScreen cmailList;
RoomScreen conversation{true};
#endif
#if RC_FEAT_FEED
FeedListScreen feedList{FeedListScreen::Kind::Feed};
FeedListScreen watched{FeedListScreen::Kind::Watched};
FeedListScreen bookmarks{FeedListScreen::Kind::Bookmarks};
#endif
#if RC_FEAT_ENTRIES
EntryScreen entry;
#endif
#if RC_FEAT_COMPOSER
ComposerScreen composer;
#endif
#if RC_FEAT_GUILDS
GuildListScreen guildList;
FeedListScreen guild{FeedListScreen::Kind::Guild};
MembersScreen members;
#endif
#if RC_FEAT_PEOPLE
ProfileScreen profile;
FeedListScreen userPosts{FeedListScreen::Kind::User};
#endif
#if RC_FEAT_UNREAD
NotificationsScreen notifications;
#endif

Id cur = Id::Splash;
Screen* active = &splash;
Place curPlace{Id::Splash, ""};
Place base{Id::Splash, ""};
// Set by goFrom: Back from `to` returns to `from`.
struct Origin {
  bool set = false;
  Id from = Id::Home, to = Id::Home;
} origin;

Screen* lookup(Id id) {
  switch (id) {
    case Id::Splash: return &splash;
    case Id::Setup: return &setup;
    case Id::Home: return &home;
    case Id::RoomList: return &roomList;
    case Id::Room: return &room;
    case Id::ComingSoon: return &comingSoon;
    case Id::LoginForm: return &loginForm;
    case Id::Settings: return &settingsScreen;
    case Id::Wifi: return &wifiScreen;
#if RC_FEAT_CMAIL
    case Id::CmailList: return &cmailList;
    case Id::Conversation: return &conversation;
#endif
#if RC_FEAT_FEED
    case Id::FeedList: return &feedList;
    case Id::Watched: return &watched;
    case Id::Bookmarks: return &bookmarks;
#endif
#if RC_FEAT_ENTRIES
    case Id::Entry: return &entry;
#endif
#if RC_FEAT_COMPOSER
    case Id::Composer: return &composer;
#endif
#if RC_FEAT_GUILDS
    case Id::GuildList: return &guildList;
    case Id::Guild: return &guild;
    case Id::Members: return &members;
#endif
#if RC_FEAT_PEOPLE
    case Id::Profile: return &profile;
    case Id::UserPosts: return &userPosts;
#endif
#if RC_FEAT_UNREAD
    case Id::Notifications: return &notifications;
#endif
    default: break;  // a screen not built: nothing goes there
  }
  return &home;
}

bool signedIn() {
#if RC_FAKE_API
  return true;
#else
  return app().auth == 1;
#endif
}

}  // namespace

void go(Id id, const std::string& arg) {
  if (id != origin.to && id != Id::Entry && id != Id::Composer) origin.set = false;
  active->leave();
  curPlace = Place{id, arg};
  if (id != Id::Entry && id != Id::Composer) base = curPlace;
  cur = id;
  active = lookup(id);
  active->enter(arg);
}

void goFrom(Id from, Id id, const std::string& arg) {
  go(id, arg);
  origin = Origin{true, from, id};
}

bool backToOrigin() {
  if (!origin.set || cur != origin.to) return false;
  go(origin.from);
  return true;
}

Id current() { return cur; }
Place here() { return curPlace; }
Place lastBase() { return base; }
void setBase(const Place& p) { base = p; }
Screen& screen() { return *active; }

bool handleGlobal(const KeyEvent& k) {
  switch (k.action) {
    case Action::OpenCirc:
    case Action::OpenHome:
#if RC_FEAT_CMAIL
    case Action::OpenCmail:
#endif
#if RC_FEAT_FEED
    case Action::OpenFeed:
#endif
#if RC_FEAT_GUILDS
    case Action::OpenMap:
#endif
      break;
    default:
      return false;
  }
  if (!signedIn() || cur == Id::Splash) return true;
  bool hadOverlay = active->hasOverlay();
  active->closeOverlays();
  switch (k.action) {
    case Action::OpenCirc:
      if (cur == Id::Room && !hadOverlay) go(Id::RoomList);
      else if (cur != Id::Room) go(Id::Room, app().cfg.lastRoom);
      break;
    case Action::OpenHome:
      go(Id::Home);
      break;
#if RC_FEAT_CMAIL
    case Action::OpenCmail:
      // From a conversation, a second press (overlays closed) goes to the list.
      if (cur != Id::Conversation || !hadOverlay) go(Id::CmailList);
      break;
#endif
#if RC_FEAT_FEED
    case Action::OpenFeed:
      // The Composer asks "Discard draft?" first, so it gets the key.
      if (cur == Id::Composer) active->onKey(k);
      // In the list, a press with a palette open only closes it.
      else if (cur != Id::FeedList || !hadOverlay) go(Id::FeedList);
      break;
#endif
#if RC_FEAT_GUILDS
    case Action::OpenMap:
      if (cur == Id::Composer) active->onKey(k);
      else if (cur != Id::GuildList || !hadOverlay) go(Id::GuildList);
      break;
#endif
    default:
      break;
  }
  return true;
}

Id bootTarget() {
#if RC_FAKE_API
  return Id::Home;
#else
  const Settings& c = app().cfg;
  if (c.wifi.empty() || c.refreshToken.empty()) return Id::Setup;
  return Id::Home;
#endif
}

}  // namespace rc::router
