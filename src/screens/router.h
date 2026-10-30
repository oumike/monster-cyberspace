#pragma once

#include <cstdint>
#include <string>

#include "app/messages.h"
#include "input/key_event.h"
#include "ui/grid.h"
#include "ui/theme.h"

namespace rc {

class Screen {
 public:
  virtual ~Screen() = default;
  virtual void enter(const std::string& arg) { (void)arg; }
  virtual void leave() {}
  virtual void onKey(const KeyEvent&) = 0;
  virtual void onEvent(const Event&) {}
  virtual void render(Grid&, const Theme&, int64_t nowMs) = 0;
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
};

struct Place {
  Id id;
  std::string arg;
};

void go(Id id, const std::string& arg = "");
Id current();
// The screen before the current one, as go() was given it.
Place previous();
// The last place go() was given that is not Entry or Composer.
Place lastBase();
Screen& screen();
// The dedicated buttons (Messages, Home, C-Mail, Feed, Map). True when the
// key was one of them.
bool handleGlobal(const KeyEvent&);
// Where the boot flow lands: Setup with no Wi-Fi or no account, else Home.
Id bootTarget();

}  // namespace router

}  // namespace rc
