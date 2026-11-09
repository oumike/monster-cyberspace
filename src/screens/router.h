#pragma once

#include <lvgl.h>

#include <cstdint>
#include <string>

#include "app/messages.h"
#include "input/key_event.h"
#include "ui/theme.h"

namespace rc {

class Screen {
 public:
  virtual ~Screen() = default;
  virtual void enter(const std::string& arg) { (void)arg; }
  virtual void leave() {}
  virtual void onKey(const KeyEvent&) = 0;
  virtual void onEvent(const Event&) {}
  // Creates the screen's objects under root (after enter), synced.
  virtual void build(lv_obj_t* root) { (void)root; }
  // Each frame, before LVGL draws: the clock, timers and changes to show.
  virtual void update(int64_t nowMs) { (void)nowMs; }
  // True while a focused text field is taking typing, so a touch board can
  // dock its on-screen keyboard. False on keyed boards (nothing reads it).
  virtual bool acceptsText() const { return false; }
  // True while a focused text input has something to delete. Boards where
  // Backspace doubles as Back (board::backspaceIsBack) treat it as Back
  // otherwise. Only answers a question; onKey is unchanged.
  virtual bool consumesBackspace() const { return false; }
  // Menus, the drawer or a selection: global keys close these first.
  virtual bool hasOverlay() const { return false; }
  virtual void closeOverlays() {}
  // The symbol table's last row, which opens this screen's palette; empty
  // when there is none.
  virtual std::u32string paletteLabel() const { return U""; }
  // Drawn in Compact text whatever the saved text size.
  virtual bool compactText() const { return false; }
};

namespace router {

enum class Id {
  Splash,
  Setup,
  Home,
  RoomList,
  Room,
  ComingSoon,
  LoginForm,
  CmailList,
  Conversation,
  FeedList,
  Entry,
  Composer,
  Watched,
  Bookmarks,
  Settings,
  Wifi,
  GuildList,
  Guild,
  Members,
  Profile,
  UserPosts,
  Notifications,  // the Unread screen
  Topics,         // every topic, from the Feed's commands
  Topic,          // a topic's entries; arg = its name
  Following,      // entries by people I follow (Home)
};

struct Place {
  Id id;
  std::string arg;
};

void go(Id id, const std::string& arg = "");
// go(id, arg), and Back from that screen (backToOrigin) returns to `from`
// (with the argument it was open with) until the user goes somewhere else.
// Entry and Composer keep it.
void goFrom(Id from, Id id, const std::string& arg = "");
// Goes back to goFrom's `from` when the current screen is the one it opened;
// false, and no move, otherwise.
bool backToOrigin();
Id current();
// Where go() last went, with its argument.
Place here();
// The last place go() was given that is not Entry or Composer.
Place lastBase();
// Puts back a lastBase() saved earlier, before returning to an Entry.
void setBase(const Place& p);
Screen& screen();
// Builds the current screen into a new root (go does it; this is for the
// first screen, and after a theme, text size, rotation or keyboard change).
// Out of memory, go keeps the screen it was on, and so does this. Without
// PSRAM both delete the old tree first, so running out twice (the new
// screen, then the old one again) can leave no tree: see hasTree().
void rebuild();
// False while no tree stands (out of memory twice, boards without PSRAM).
// The screen's state is still there and takes events; its views are not,
// so nothing may call update() or send it keys until update() has built
// it again.
bool hasTree();
// Each frame: the screen's update() while it has a tree; with none, builds
// it again at most once a second (until memory is back) instead.
void update(int64_t nowMs);
// Called before every build, with screen() already the screen to build: the
// UI task sets the root font for it there.
void setBeforeBuild(void (*hook)());
// The dedicated buttons (Messages, Home, C-Mail, Feed, Map). True when the
// key was one of them.
bool handleGlobal(const KeyEvent&);
// Where the boot flow lands: Setup with no Wi-Fi or no account, else Home.
Id bootTarget();

}  // namespace router

}  // namespace rc
