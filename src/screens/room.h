#pragma once

#include <set>
#include <string>
#include <vector>

#include "chat/message.h"
#include "screens/link_view.h"
#include "screens/router.h"
#include "ui/widgets/drawer.h"
#include "ui/widgets/input_line.h"
#include "ui/widgets/list_view.h"
#include "ui/widgets/log_view.h"
#include "ui/widgets/overlay.h"

namespace rc {

// The cIRC room (spec §5–§7): the log, the input line, the who's-here drawer,
// the palette, the room, person and message menus, a reader and a link view.
// With dm, a C-Mail conversation instead: no drawer, a typing indicator, and
// no reply, delete or report.
class RoomScreen : public Screen {
 public:
  explicit RoomScreen(bool dm = false) : dm_(dm) {}
  // slug: a room slug, or a conversation id when dm.
  void enter(const std::string& slug) override;
  void leave() override;
  void onKey(const KeyEvent&) override;
  void onEvent(const Event&) override;
  void render(Grid&, const Theme&, int64_t nowMs) override;
  bool hasOverlay() const override;
  void closeOverlays() override;
  bool acceptsText() const override { return view_ == View::None; }
  std::u32string paletteLabel() const override { return U"commands\u2026"; }
  bool consumesBackspace() const override {
    return view_ == View::None && !input_.text().empty();
  }

 private:
  enum class View { None, Menu, Reader, Link };
  enum class Menu { Room, Person, Message, MutePick, UnmutePick, Palette, ConfirmDelete, ConfirmReport };

  // Keys by mode.
  void typingKey(const KeyEvent&);
  void menuKey(const KeyEvent&);
  void viewerKey(const KeyEvent&);
  void reasonKey(const KeyEvent&);

  void send();
  void openMenu(Menu m, std::u32string title, std::vector<ListItem> items);
  void openRoomMenu();
  void openPersonMenu(const std::string& name);
  void openMessageMenu();
  void pickMenu();
  void setDrawer(bool open);
  void openReader(const CircMessage& m);
  void openLink(const std::string& url);
  bool isMuted(const std::string& name);
  void insertText(const std::u32string& s);
  bool drawerOpen() const;
  // A wide panel keeps the who's-here list docked on the right of a room,
  // whether or not the drawer has the keys.
  bool drawerDocked() const;
  // Typing on/off writes, only ever from a key press (C-Mail).
  void noteTyping();
  void stopTyping();

  const bool dm_;
  std::string slug_;
  std::string partner_;         // the other person, when dm
  bool typingOn_ = false;       // we told the server we're typing
  int64_t lastTypingPost_ = 0;
  LogView log_;
  InputLine input_;
  Drawer drawer_;
  Overlay box_;
  View view_ = View::None;
  Menu menu_ = Menu::Room;
  std::vector<std::string> menuKeys_;  // the action or name behind each item
  std::string target_;                 // the person or message a menu acts on

  // Reader and link views.
  std::vector<std::u32string> viewLines_;
  LinkView link_;
  int viewTop_ = 0, viewLeft_ = 0;
  bool wrapView_ = true;  // wrap text to the width; art is drawn verbatim

  // Report reason entry, which borrows the input line.
  bool reasonMode_ = false;
  std::u32string savedInput_;

  std::u32string pending_;  // text waiting for SendDone
  bool olderAsked_ = false;
  uint32_t version_ = UINT32_MAX;
  int cols_ = 0;
  bool relayout_ = true;
  const Theme* theme_ = nullptr;  // the lines bake its colours
  int64_t lastMarkRead_ = 0;
  std::set<std::string> muted_;  // this room's mutes, lower-cased
  std::string self_;
};

}  // namespace rc
