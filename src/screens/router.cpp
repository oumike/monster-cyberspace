#include "screens/router.h"

#include "app/app_state.h"
#include "screens/room.h"
#include "screens/screens.h"

namespace rc::router {

namespace {

SplashScreen splash;
SetupScreen setup;
HomeScreen home;
RoomListScreen roomList;
RoomScreen room;
CmailListScreen cmailList;
RoomScreen conversation{true};
ComingSoonScreen comingSoon;
LoginFormScreen loginForm;

Id cur = Id::Splash;
Screen* active = &splash;

Screen* lookup(Id id) {
  switch (id) {
    case Id::Splash: return &splash;
    case Id::Setup: return &setup;
    case Id::Home: return &home;
    case Id::RoomList: return &roomList;
    case Id::Room: return &room;
    case Id::ComingSoon: return &comingSoon;
    case Id::LoginForm: return &loginForm;
    case Id::CmailList: return &cmailList;
    case Id::Conversation: return &conversation;
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
  active->leave();
  cur = id;
  active = lookup(id);
  active->enter(arg);
}

Id current() { return cur; }
Screen& screen() { return *active; }

bool handleGlobal(const KeyEvent& k) {
  switch (k.action) {
    case Action::OpenCirc:
    case Action::OpenHome:
    case Action::OpenCmail:
    case Action::OpenFeed:
    case Action::OpenMap:
      break;
    default:
      return false;
  }
  if (k.action == Action::OpenMap || !signedIn() || cur == Id::Splash) return true;
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
    case Action::OpenCmail:
      // From a conversation, a second press (overlays closed) goes to the list.
      if (cur != Id::Conversation || !hadOverlay) go(Id::CmailList);
      break;
    case Action::OpenFeed:
      go(Id::ComingSoon, "Feed");
      break;
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
