#pragma once

#include "input/type_filter.h"
#include "net/wifi_list.h"
#include "screens/backup_picker.h"
#include "screens/button_pad.h"
#include "screens/guild_info.h"
#include "screens/router.h"
#include "ui/home_tiles.h"
#include "ui/widgets/entry_list.h"
#include "ui/widgets/input_line.h"
#include "ui/widgets/list_view.h"
#include "ui/widgets/overlay.h"

namespace rc {

class SplashScreen : public Screen {
 public:
  void enter(const std::string&) override;
  void onKey(const KeyEvent&) override;
  void render(Grid&, const Theme&, int64_t nowMs) override;

 private:
  int64_t since_ = -1;
  const char* say_ = "";
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
  void renderTiles(Grid&, const Theme&, const std::vector<TileRect>&, int cellW);
  ListView menu_;
  int tileSel_ = 0;      // selected tile (touch boards)
  bool tileMode_ = false;  // set each render: tiles shown this frame
  bool tileHidden_ = false;  // touch hides the tile cursor until an arrow or Select
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
  ButtonPad pad_;  // touch-only boards: rooms as buttons
  bool buttons_ = false;
};

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

// The Feed, Watched threads and Bookmarks lists: one class, three router
// instances (spec §5).
class FeedListScreen : public Screen {
 public:
  enum class Kind { Feed, Watched, Bookmarks, Guild };
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
  std::string slug_;           // Guild: the open guild
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
  bool more_ = false, asked_ = false, failed_ = false;
};

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

class ComingSoonScreen : public Screen {
 public:
  void enter(const std::string& name) override { name_ = name; }
  void onKey(const KeyEvent&) override;
  void render(Grid&, const Theme&, int64_t nowMs) override;

 private:
  std::string name_;
};

}  // namespace rc
