#pragma once

#include <set>
#include <string>
#include <vector>

#include "chat/message.h"
#include "feature_flags.h"
#include "lvui/tree_views.h"
#include "lvui/views/button_rows.h"
#include "lvui/views/cell_text.h"
#include "lvui/views/drawer_panel.h"
#include "lvui/views/input_field.h"
#include "lvui/views/log_box.h"
#include "lvui/views/menu_overlay.h"
#include "lvui/views/qr_box.h"
#include "lvui/views/top_bar.h"
#include "screens/link_view.h"
#include "screens/lv_draw.h"
#include "screens/router.h"
#include "ui/widgets/drawer.h"
#include "ui/widgets/input_line.h"
#include "ui/widgets/list_view.h"
#include "ui/widgets/log_view.h"
#include "ui/widgets/overlay.h"
#include "ui/widgets/spell_strip.h"

namespace rc {

// The cIRC room (spec §5–§7): the log, the input line, the who's-here drawer,
// the palette, the room, person and message menus, a reader and a link view.
// With dm, a C-Mail conversation instead: no drawer, a typing indicator, and
// no reply, delete or report.
class RoomScreen : public Screen {
 public:
#if RC_FEAT_CMAIL
  explicit RoomScreen(bool dm = false) : dm_(dm) {}
#else
  RoomScreen() = default;
#endif
  // slug: a room slug, or a conversation id when dm.
  void enter(const std::string& slug) override;
  void leave() override;
  void resume() override;
  void onKey(const KeyEvent&) override;
  void onEvent(const Event&) override;
  void build(lv_obj_t* root) override;
  void update(int64_t nowMs) override;
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
  void setUsersHidden(bool hide);
  void showUsers();
  void openReader(const CircMessage& m);
  void openLink(const std::string& url);
  bool isMuted(const std::string& name);
  void insertText(const std::u32string& s);
  bool drawerOpen() const;
  // A wide panel keeps the who's-here list docked on the right of a room,
  // whether or not the drawer has the keys.
  bool drawerDocked() const;
  bool drawerDockable() const;
  // Typing on/off writes, only ever from a key press (C-Mail).
  void noteTyping();
  void stopTyping();

#if RC_FEAT_CMAIL
  const bool dm_;
#else
  static constexpr bool dm_ = false;  // without C-Mail every RoomScreen is a room
#endif
  std::string slug_;
  std::string partner_;         // the other person, when dm
  bool typingOn_ = false;       // we told the server we're typing
  int64_t lastTypingPost_ = 0;
  LogView log_;
  LayoutOptions logOptions(const std::string& self, const std::set<std::string>& revealed) const;
  std::vector<DisplayLine> pendingLines() const;
  void fillLog(const std::string& first, const std::string& last, const LogView::Out& out);
  // What a frame takes from the chat state, under the lock: the presence
  // (into the drawer when shown), the typers, this room's mutes, a mark read
  // (sent once the lock is let go) and the log laid out again when its
  // inputs changed. sync runs it. sameTheme: the theme the
  // lines bake is still t's.
  struct Frame {
    int online = 0;
    std::vector<std::string> typers;
  };
  Frame refresh(const Theme& t, int logCols, bool shown, bool sameTheme);
  // The hint row for the typing view, keys and touch.
  void hints(bool drawer, bool docked, bool offline, const char32_t*& keys, const char32_t*& touch) const;
  void sync();
  void syncReader(const Theme& t);
  InputLine input_;
  SpellStrip spell_;
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

  unsigned themeSerial_ = ~0u;  // lvui::themeSerial() the lines bake
  // The views, on the heap only while their tree is up (lvui/tree_views.h).
  // The objects held here go with them: a tree thrown away gives back the
  // old tree's. body_ holds the rows between the status bar and what sits
  // under the log, for the views that fill their parent.
  struct Views {
    lv_obj_t* root_ = nullptr;
    lv_obj_t* body_ = nullptr;
    lv_obj_t* readerBox_ = nullptr;  // the reader's rows, for its scroll bar; null until it opens
    int32_t readerIndex_ = 0;        // readerBox_'s place among root_'s children
    SyncGate<6> gate_;  // the update check, fresh with each tree
    lvui::TopBar status_;
    lvui::CellText text_;
    lvui::LogBox logView_;
    lvui::DrawerPanel drawerView_;
    SideNavHost navView_;
    lvui::SpellBar spellView_;
    lvui::InputField inputView_;
    lvui::QrBox qrView_;
    lvui::ScrollBarView readerBar_;
    lvui::HintBar hint_;
    lvui::MenuOverlay menuView_;
  };
  lvui::TreeViews<Views> v_;
};

}  // namespace rc
