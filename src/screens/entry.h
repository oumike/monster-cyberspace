#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "screens/link_view.h"
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
  void onKey(const KeyEvent&) override;
  void onEvent(const Event&) override;
  void render(Grid&, const Theme&, int64_t nowMs) override;
  bool hasOverlay() const override;
  void closeOverlays() override;
  bool acceptsText() const override { return view_ == View::None; }
  bool consumesBackspace() const override {
    return view_ == View::None && !input_.text().empty();
  }

 private:
  enum class View { None, Menu, Link };
  enum class Menu { Entry, Reply, ConfirmDelete, ConfirmReport };

  void typingKey(const KeyEvent&);
  void menuKey(const KeyEvent&);
  void reasonKey(const KeyEvent&);
  void down();
  void select();
  void send();
  void openMenu(Menu m, std::u32string title, std::vector<ListItem> items);
  void openEntryMenu();
  void openReplyMenu(const std::string& replyId);
  void pickMenu();
  void rebuild(int cols, int logRows, const Theme& t);
  void selectBlock(const std::string& id);

  // The selected reply's id (any page of it); "" for the body or following.
  std::string selectedReply() const;

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

  // Report reason entry, which borrows the input line.
  bool reasonMode_ = false;
  std::u32string savedInput_;

  bool loaded_ = false;       // the entry is in the store
  bool hidden_ = false;       // a CW/NSFW body not yet revealed
  bool revealed_ = false;
  bool sending_ = false;      // a reply waits for FeedWriteDone
  bool moreAsked_ = false;    // a MoreReplies is outstanding
  std::u32string title_;
  std::map<std::string, std::string> authors_;  // reply id → author

  bool dirty_ = true;
  int cols_ = 0, logRows_ = 0;
  TextSize size_ = TextSize::Compact;
  int64_t builtAt_ = 0;
  const Theme* theme_ = nullptr;  // the lines bake its colours
};

}  // namespace rc
