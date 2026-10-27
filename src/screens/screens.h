#pragma once

#include "screens/router.h"
#include "ui/widgets/entry_list.h"
#include "ui/widgets/input_line.h"
#include "ui/widgets/list_view.h"

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

 private:
  ListView menu_;
};

class LoginFormScreen : public Screen {
 public:
  void enter(const std::string&) override;
  void leave() override;
  void onKey(const KeyEvent&) override;
  void onEvent(const Event&) override;
  void render(Grid&, const Theme&, int64_t nowMs) override;

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
  ListView menu_;
};

// On-device settings: the setup AP, log out, factory reset.
class SettingsScreen : public Screen {
 public:
  void enter(const std::string&) override;
  void onKey(const KeyEvent&) override;
  void render(Grid&, const Theme&, int64_t nowMs) override;
  bool hasOverlay() const override { return confirm_ != Confirm::None || picking_; }
  void closeOverlays() override {
    confirm_ = Confirm::None;
    picking_ = false;
  }

 private:
  enum class Confirm { None, Logout, Reset };
  void openZones();
  void zoneKey(const KeyEvent&);

  ListView menu_, confirmMenu_, zoneList_;
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
  ListView paletteList_;
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
