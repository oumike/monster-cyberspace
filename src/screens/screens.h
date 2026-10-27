#pragma once

#include "screens/backup_picker.h"
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
};

class SetupScreen : public Screen {
 public:
  void enter(const std::string&) override;
  void onKey(const KeyEvent&) override;
  void render(Grid&, const Theme&, int64_t nowMs) override;
  bool hasOverlay() const override { return picker_.isOpen(); }
  void closeOverlays() override { picker_.close(); }

 private:
  ListView menu_;
  BackupPicker picker_;
  bool hasRestore_ = false;  // the SD list held backups on enter
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

 private:
  enum class Confirm { None, Logout, Reset, Rotation };
  void openZones();
  void zoneKey(const KeyEvent&);
  void confirmKey(const KeyEvent&);

  ListView menu_, zoneList_;
  Overlay confirmBox_;
  BackupPicker picker_;
  Confirm confirm_ = Confirm::None;
  bool picking_ = false;  // the time zone list is open
};

class RoomListScreen : public Screen {
 public:
  void enter(const std::string&) override;
  void onKey(const KeyEvent&) override;
  void render(Grid&, const Theme&, int64_t nowMs) override;

 private:
  ListView list_;
  std::vector<std::string> slugs_;
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

  ListView list_;
  std::vector<std::string> ids_;
  InputLine input_;  // the username for a new conversation
  bool editing_ = false, busy_ = false;
};

// The Feed, Watched threads and Bookmarks lists: one class, three router
// instances (spec §5).
class FeedListScreen : public Screen {
 public:
  enum class Kind { Feed, Watched, Bookmarks };
  explicit FeedListScreen(Kind kind) : kind_(kind) {}
  void enter(const std::string&) override;
  void leave() override;
  void onKey(const KeyEvent&) override;
  void onEvent(const Event&) override;
  void render(Grid&, const Theme&, int64_t nowMs) override;
  bool hasOverlay() const override { return palette_; }
  void closeOverlays() override { palette_ = false; }

 private:
  void paletteKey(const KeyEvent&);
  void open();         // the first page
  void requestMore();  // the next page
  void rebuild(int cols, const Theme& t);
  void remember();     // selId_ from the selection

  const Kind kind_;
  EntryList list_;
  Overlay box_;
  bool palette_ = false;
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

class ComingSoonScreen : public Screen {
 public:
  void enter(const std::string& name) override { name_ = name; }
  void onKey(const KeyEvent&) override;
  void render(Grid&, const Theme&, int64_t nowMs) override;

 private:
  std::string name_;
};

}  // namespace rc
