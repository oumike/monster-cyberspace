#pragma once

#include <utility>
#include <vector>

#include "features.h"
#include "input/type_filter.h"
#include "net/wifi_list.h"
#include "screens/backup_picker.h"
#include "screens/button_pad.h"
#if RC_FEAT_ENTRIES
#include "screens/guild_info.h"
#endif
#include "screens/link_view.h"
#include "screens/router.h"
#include "ui/home_tiles.h"
#if RC_FEAT_UNREAD
#include "ui/unread_inbox.h"
#endif
#include "ui/widgets/entry_list.h"
#include "ui/widgets/input_line.h"
#include "ui/widgets/list_view.h"
#include "ui/widgets/overlay.h"
#include "ui/widgets/text_view.h"

namespace rc {

class SplashScreen : public Screen {
 public:
  void enter(const std::string&) override;
  void onKey(const KeyEvent&) override;
  void render(Grid&, const Theme&, int64_t nowMs) override;

 private:
  int64_t since_ = -1;
  std::string say_;
};

class SetupScreen : public Screen {
 public:
  void enter(const std::string&) override;
  void onKey(const KeyEvent&) override;
  void render(Grid&, const Theme&, int64_t nowMs) override;
  bool hasOverlay() const override { return picker_.isOpen(); }
  void closeOverlays() override { picker_.close(); }

 private:
  void activate(int item);
  ListView menu_;
  BackupPicker picker_;
  bool hasRestore_ = false;  // the SD list held backups on enter
  bool buttons_ = false;     // touch-only boards: the options as buttons
  ButtonPad pad_;
};

class LoginFormScreen : public Screen {
 public:
  void enter(const std::string&) override;
  void leave() override;
  void onKey(const KeyEvent&) override;
  void onEvent(const Event&) override;
  void render(Grid&, const Theme&, int64_t nowMs) override;
  bool acceptsText() const override { return !busy_; }
  bool consumesBackspace() const override {
    return !busy_ && !(focus_ == 0 ? email_ : password_).text().empty();
  }

 private:
  InputLine email_, password_;
  int focus_ = 0;  // 0 email, 1 password
  bool busy_ = false;
  std::string note_;
};

class HomeScreen : public Screen {
 public:
  void enter(const std::string&) override;
  void onKey(const KeyEvent&) override;
  void render(Grid&, const Theme&, int64_t nowMs) override;

 private:
  void openItem(int item);
  // cellW: the tiles' cell width in px when drawn on RowKind::Big rows, else 0.
  // tiles are slots for tiles first.. on; a column (homeColumn) scrolls.
  void renderTiles(Grid&, const Theme&, const std::vector<TileRect>&, int cellW, int first,
                   bool column);
  ListView menu_;
  int tileSel_ = 0;      // selected tile (touch boards)
  bool tileMode_ = false;  // set each render: tiles shown this frame
  bool tileHidden_ = false;  // touch hides the tile cursor until an arrow or Select
  // The single column (homeColumn): the top tile shown, how many fit, rows
  // from one tile to the next, and touch Up/Down steps (a row each) not yet
  // a whole tile.
  bool column_ = false;
  int colTop_ = 0, colVisible_ = 1, colStride_ = 3, colCarry_ = 0;
};

// On-device settings: the setup AP, log out, factory reset.
class SettingsScreen : public Screen {
 public:
  void enter(const std::string&) override;
  void onKey(const KeyEvent&) override;
  void render(Grid&, const Theme&, int64_t nowMs) override;
  bool hasOverlay() const override {
    return confirm_ != Confirm::None || picking_ || picker_.isOpen();
  }
  void closeOverlays() override {
    confirm_ = Confirm::None;
    picking_ = false;
    picker_.close();
  }
  // Larger sizes cut off the buttons' text.
  bool compactText() const override { return true; }

 private:
  enum class Confirm {
    None, Logout, Reset, Rotation, TextSize, Timeout, KbLight, NotifyLight, NotifyTimeout, Theme,
    Update, Idle, Wake
  };
  void activate(int item);
  void openZones();
  void zoneKey(const KeyEvent&);
  void confirmKey(const KeyEvent&);

  ListView menu_, zoneList_;
  Overlay confirmBox_;
  BackupPicker picker_;
  Confirm confirm_ = Confirm::None;
  bool picking_ = false;  // the time zone list is open
  ButtonPad pad_;         // touch boards: the menu as buttons
  bool buttons_ = false;
};

// Wi-Fi on the device: scan, join (with a password, or a name typed for a
// hidden network), and Connect / New password / Forget on saved ones. Its
// arg "setup" comes from the setup screen, which Back and a join return to.
class WifiScreen : public Screen {
 public:
  void enter(const std::string& arg) override;
  void leave() override;
  void onKey(const KeyEvent&) override;
  void onEvent(const Event&) override;
  void render(Grid&, const Theme&, int64_t nowMs) override;
  bool hasOverlay() const override { return menu_.isOpen() || typing(); }
  void closeOverlays() override;
  bool acceptsText() const override { return typing(); }
  bool consumesBackspace() const override { return typing() && !input_.text().empty(); }
  bool compactText() const override { return ButtonPad::active(); }

 private:
  enum class Mode { List, Name, Password, Joining };
  bool typing() const { return mode_ == Mode::Name || mode_ == Mode::Password; }
  void scan();
  void pick(int index);
  void menuKey(const KeyEvent&);
  void entryKey(const KeyEvent&);
  void startEntry(Mode m, const std::string& ssid);
  // password null: join with the saved one.
  void join(const std::string& ssid, const std::string* password);
  void setNote(const std::string& text, bool error);

  Mode mode_ = Mode::List;
  ListView list_;
  ButtonPad pad_;
  bool buttons_ = false;
  Overlay menu_;
  InputLine input_;
  std::string ssid_;  // the network being joined or edited
  std::vector<WifiScanEntry> scan_;
  std::vector<WifiRow> rows_;
  bool scanning_ = false, fromSetup_ = false;
  bool added_ = false;  // the join saved a network that wasn't saved before
  std::string note_;
  bool noteError_ = false;
};

class RoomListScreen : public Screen {
 public:
  void enter(const std::string&) override;
  void onKey(const KeyEvent&) override;
  void render(Grid&, const Theme&, int64_t nowMs) override;
  bool consumesBackspace() const override { return filter_.active(); }

 private:
  void open(int index);
  ListView list_;
  std::vector<std::string> slugs_;
  TypeFilter filter_;  // keyboards: typing narrows the rooms
  ButtonPad pad_;  // touch boards: rooms as buttons (a cursor with keys)
  bool buttons_ = false;
};

#if RC_FEAT_CMAIL
// C-Mail: "New conversation…" and the conversations, unread first.
class CmailListScreen : public Screen {
 public:
  void enter(const std::string&) override;
  void onKey(const KeyEvent&) override;
  void onEvent(const Event&) override;
  void render(Grid&, const Theme&, int64_t nowMs) override;
  bool hasOverlay() const override { return editing_; }
  void closeOverlays() override { editing_ = busy_ = false; }
  bool acceptsText() const override { return editing_ && !busy_; }
  bool consumesBackspace() const override {
    return editing_ && !busy_ && !input_.text().empty();
  }

 private:
  void editKey(const KeyEvent&);
  void open(int index);

  ListView list_;
  ButtonPad pad_;
  bool buttons_ = false;  // touch only: conversations are buttons
  std::vector<std::string> ids_;
  InputLine input_;  // the username for a new conversation
  bool editing_ = false, busy_ = false;
};
#endif

#if RC_FEAT_ENTRIES
// The Feed, Watched threads and Bookmarks lists: one class, three router
// instances (spec §5).
class FeedListScreen : public Screen {
 public:
  enum class Kind { Feed, Watched, Bookmarks, Guild, User };
  explicit FeedListScreen(Kind kind) : kind_(kind) {}
  void enter(const std::string&) override;
  void leave() override;
  void onKey(const KeyEvent&) override;
  void onEvent(const Event&) override;
  void render(Grid&, const Theme&, int64_t nowMs) override;
  bool hasOverlay() const override { return palette_ || info_.isOpen(); }
  void closeOverlays() override {
    palette_ = confirm_ = false;
    box_.close();
    info_.close();
  }

 private:
  void paletteKey(const KeyEvent&);
  void openPalette();
  std::u32string guildTitle() const;
  void askConfirm(CmdType type, const Guild& g);
  void open();         // the first page
  void requestMore();  // the next page
  void rebuild(int cols, const Theme& t);
  void remember();     // selId_ from the selection

  const Kind kind_;
  EntryList list_;
  Overlay box_;
  bool palette_ = false;
  bool confirm_ = false;       // box_ holds a membership confirm
  CmdType pending_ = CmdType::OpenGuilds;  // the command that confirm sends
  std::vector<int> acts_;      // the palette's items, as PaletteItem values
  std::string slug_;           // Guild: the open guild; User: the username
  GuildInfoView info_;
  std::vector<std::string> ids_;
  std::vector<std::string> replyIds_;  // Bookmarks: the bookmarked reply per row, or ""
  std::string selId_;      // the selected entry, kept across updates
  bool dirty_ = true;      // rows need a rebuild
  bool loaded_ = false;    // a list arrived since enter
  bool failed_ = false;    // a side list failed before it ever loaded
  bool more_ = false;      // more pages remain
  bool asked_ = false;     // a page request is outstanding
  int cols_ = 0;
  int64_t builtAt_ = 0;    // for the ages ("3h")
  const Theme* theme_ = nullptr;  // the rows bake its colours
};
#endif

#if RC_FEAT_GUILDS
// A guild's members, read-only and paged (spec §5).
class MembersScreen : public Screen {
 public:
  void enter(const std::string& slug) override;
  void onKey(const KeyEvent&) override;
  void onEvent(const Event&) override;
  void render(Grid&, const Theme&, int64_t nowMs) override;

 private:
  void requestMore();
  ListView list_;
  std::string slug_;
  std::vector<std::string> names_;  // usernames by row, for Select
  bool more_ = false, asked_ = false, failed_ = false;
};
#endif

#if RC_FEAT_PEOPLE
// A user's profile (People spec): scrolling text; Centre or the menu opens
// its actions. Back returns where it was opened from (people::back).
class ProfileScreen : public Screen {
 public:
  void enter(const std::string& username) override;
  void leave() override { closeOverlays(); }
  void onKey(const KeyEvent&) override;
  void onEvent(const Event&) override;
  void render(Grid&, const Theme&, int64_t nowMs) override;
  bool hasOverlay() const override { return menu_ || linkOpen_; }
  void closeOverlays() override {
    menu_ = confirm_ = linkOpen_ = false;
    box_.close();
  }

 private:
  void openMenu();
  void menuKey(const KeyEvent&);
  void ask(int act);  // the confirm before a follow, unfollow or poke
  void send(int act);

  std::string user_;
  TextView text_;
  std::u32string shown_;   // what text_ holds
  const Theme* styledFor_ = nullptr;  // the theme text_'s colours came from
  Overlay box_;
  LinkView link_;
  std::vector<int> acts_;  // the menu's rows, as ProfileAct values
  int pending_ = 0;        // the act the confirm sends
  bool menu_ = false, confirm_ = false, linkOpen_ = false;
  bool loaded_ = false, failed_ = false, missing_ = false;
  bool busy_ = false;       // a follow, unfollow or poke is in flight
  bool starting_ = false;   // a StartConversation is in flight
  bool dirty_ = true;
  int64_t builtAt_ = 0;
};
#endif

#if RC_FEAT_GUILDS
// Every guild: mine first (pinned), then the browse list (spec §5).
class GuildListScreen : public Screen {
 public:
  void enter(const std::string&) override;
  void leave() override { palette_ = false; }
  void onKey(const KeyEvent&) override;
  void onEvent(const Event&) override;
  void render(Grid&, const Theme&, int64_t nowMs) override;
  bool hasOverlay() const override { return palette_; }
  void closeOverlays() override { palette_ = false; }

 private:
  void paletteKey(const KeyEvent&);
  void open();
  void requestMore();
  void step(bool down);  // moves the selection, skipping the dividers
  void rebuild(int cols, const Theme& t);
  void remember();

  EntryList list_;
  Overlay box_;
  bool palette_ = false;
  std::vector<std::string> slugs_;  // per row; "" for a divider
  std::string selSlug_;
  bool dirty_ = true, failed_ = false, more_ = false, asked_ = false;
  int cols_ = 0;
  const Theme* theme_ = nullptr;
};
#endif

#if RC_FEAT_UNREAD
// Everything the Unread screen lists, newest first.
std::vector<UnreadItem> gatherUnread();

// Unread: unread C-Mail conversations, cIRC mentions and Feed/guild notices
// together, newest first (ui/unread_inbox.h). Back from what it opens comes
// back here.
class NotificationsScreen : public Screen {
 public:
  void enter(const std::string&) override {
    closeOverlays();
    marking_ = false;
  }
  void leave() override { closeOverlays(); }
  void onKey(const KeyEvent&) override;
  void onEvent(const Event&) override;
  void render(Grid&, const Theme&, int64_t nowMs) override;
  bool hasOverlay() const override { return confirm_; }
  void closeOverlays() override {
    confirm_ = false;
    box_.close();
  }

 private:
  // Row 0 is "Mark all read" when anything is unread; items follow it.
  void open(int row);
  void askMarkAll();
  ListView list_;
  Overlay box_;
  std::vector<UnreadItem> items_;  // as drawn
  bool confirm_ = false;  // box_ asks to mark all read
  bool marking_ = false;  // a MarkAllRead is in flight
};
#endif

class ComingSoonScreen : public Screen {
 public:
  void enter(const std::string& name) override { name_ = name; }
  void onKey(const KeyEvent&) override;
  void render(Grid&, const Theme&, int64_t nowMs) override;

 private:
  std::string name_;
};

}  // namespace rc
