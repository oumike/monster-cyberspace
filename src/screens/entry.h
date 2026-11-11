#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "lvui/tree_views.h"
#include "lvui/views/button_rows.h"
#include "lvui/views/cell_text.h"
#include "lvui/views/input_field.h"
#include "lvui/views/log_box.h"
#include "lvui/views/menu_overlay.h"
#include "lvui/views/qr_box.h"
#include "lvui/views/top_bar.h"
#include "screens/link_view.h"
#include "screens/lv_draw.h"
#include "screens/router.h"
#include "ui/theme.h"
#include "ui/widgets/input_line.h"
#include "ui/widgets/list_view.h"
#include "ui/widgets/log_view.h"
#include "ui/widgets/overlay.h"
#include "ui/widgets/spell_strip.h"

namespace rc {

// One Feed entry (spec §5): the header and body, then the replies in the room
// log style, with a reply line below. It opens at the top. Up/Down move over
// the blocks (the body, then each reply; a long one in screen-sized pages);
// Select sends, reveals a content warning, or opens the entry or reply menu.
class EntryScreen : public Screen {
 public:
  // arg: "postId", "postId\nnoticeId" or "postId\nnoticeId\nreplyId" (the
  // reply to select once loaded; noticeId may be empty).
  void enter(const std::string& arg) override;
  void leave() override;
  void resume() override;
  void onKey(const KeyEvent&) override;
  void onEvent(const Event&) override;
  void build(lv_obj_t* root) override;
  void update(int64_t nowMs) override;
  bool hasOverlay() const override;
  void closeOverlays() override;
  bool acceptsText() const override { return view_ == View::None; }
  bool consumesBackspace() const override {
    return view_ == View::None && !input_.text().empty();
  }

 private:
  void open();  // asks for the entry, from scratch
  enum class View { None, Menu, Link };
  enum class Menu { Entry, Reply, ConfirmDelete, ConfirmReport };

  void typingKey(const KeyEvent&);
  void menuKey(const KeyEvent&);
  void reasonKey(const KeyEvent&);
  void down();
  void select();
  void send();
  void sendEdit();     // the reply being edited (editReplyId_)
  void stopEditing();  // restores the input from before Edit
  void editTarget();   // Edit picked in a menu
  void openMenu(Menu m, std::u32string title, std::vector<ListItem> items);
  void openEntryMenu();
  void openReplyMenu(const std::string& replyId);
  void pickMenu();
  void rebuild(int cols, int logRows, const Theme& t);
  // rebuild when sync needs it: on a store event (dirty_), a new
  // size or theme (sameTheme false), or for the ages.
  void refresh(int logCols, int logRows, const Theme& t, int64_t nowMs, bool sameTheme);
  // The reply line's "↳ @author" for a selected reply.
  std::string replyLine(const std::string& reply) const;
  void hints(const std::string& reply, bool offline, const char32_t*& hint,
             const char32_t*& touch) const;
  void sync(int64_t nowMs);
  void selectBlock(const std::string& id);

  // The selected reply's id (any page of it); "" for the body or following.
  std::string selectedReply() const;
  void leaveToList();  // the guild it was opened from, else the Feed list

  std::string postId_, noticeId_;
  std::string wantReply_;     // a reply to select once the replies arrive
  bool repliesArrived_ = false;
  LogView log_;
  InputLine input_;
  SpellStrip spell_;
  Overlay box_;
  LinkView link_;
  View view_ = View::None;
  Menu menu_ = Menu::Entry;
  std::vector<std::string> menuKeys_;
  std::string target_;        // the entry or reply a menu acts on
  bool targetReply_ = false;  // target_ is a reply
  std::string url_;           // the target's link, for Show link
  std::string author_;        // the target's author, for Author's profile

  // Report reason entry, which borrows the input line.
  bool reasonMode_ = false;
  std::u32string savedInput_;

  bool loaded_ = false;       // the entry is in the store
  bool hidden_ = false;       // a CW/NSFW body not yet revealed
  bool revealed_ = false;
  bool sending_ = false;      // a reply waits for FeedWriteDone
  std::string editReplyId_;   // set: input_ holds this reply's new text
  std::u32string editSavedInput_;  // what the line held before Edit (savedInput_ is reason mode's)
  bool moreAsked_ = false;    // a MoreReplies is outstanding
  std::u32string title_;
  std::map<std::string, std::string> authors_;  // reply id → author

  bool dirty_ = true;
  int cols_ = 0, logRows_ = 0;
  TextSize size_ = TextSize::Compact;
  ChatStyle style_ = ChatStyle::Irc;
  int64_t builtAt_ = 0;
  const Theme* theme_ = nullptr;  // the lines bake its colours

  unsigned themeSerial_ = ~0u;  // lvui::themeSerial() the lines bake
  int64_t nowMs_ = 0;           // the last update's, for build
  // The views, on the heap only while their tree is up (lvui/tree_views.h).
  // The objects held here go with them: a tree thrown away gives back the
  // old tree's. body_ holds the log and the side strip between the status
  // bar and what sits above the input.
  struct Views {
    lv_obj_t* root_ = nullptr;
    lv_obj_t* body_ = nullptr;
    SyncGate<5> gate_;  // the update check, fresh with each tree
    lvui::TopBar status_;
    lvui::CellText text_;
    lvui::LogBox logView_;
    SideNavHost navView_;
    lvui::SpellBar spellView_;
    lvui::InputField inputView_;
    lvui::QrBox qrView_;
    lvui::HintBar hint_;
    lvui::MenuOverlay menuView_;
  };
  lvui::TreeViews<Views> v_;
};

}  // namespace rc
