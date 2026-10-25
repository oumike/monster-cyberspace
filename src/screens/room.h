#pragma once

#include <set>
#include <string>
#include <vector>

#include "chat/message.h"
#include "screens/router.h"
#include "ui/widgets/drawer.h"
#include "ui/widgets/input_line.h"
#include "ui/widgets/list_view.h"
#include "ui/widgets/log_view.h"

namespace rc {

// The cIRC room (spec §5–§7): the log, the input line, the who's-here drawer,
// the palette, the room, person and message menus, a reader and a link view.
class RoomScreen : public Screen {
 public:
  void enter(const std::string& slug) override;
  void leave() override;
  void onKey(const KeyEvent&) override;
  void onEvent(const Event&) override;
  void render(Grid&, const Theme&, int64_t nowMs) override;
  bool hasOverlay() const override;
  void closeOverlays() override;

 private:
  enum class Overlay { None, Menu, Reader, Link };
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

  std::string slug_;
  LogView log_;
  InputLine input_;
  Drawer drawer_;
  ListView menuList_;
  Overlay overlay_ = Overlay::None;
  Menu menu_ = Menu::Room;
  std::u32string menuTitle_;
  std::vector<std::string> menuKeys_;  // the action or name behind each item
  std::string target_;                 // the person or message a menu acts on

  // Reader and link views.
  std::vector<std::u32string> viewLines_;
  std::vector<std::u32string> qr_;
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
  int64_t lastMarkRead_ = 0;
  std::set<std::string> muted_;  // this room's mutes, lower-cased
  std::string self_;
};

}  // namespace rc
