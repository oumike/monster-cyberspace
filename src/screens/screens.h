#pragma once

#include "screens/router.h"
#include "ui/widgets/input_line.h"
#include "ui/widgets/list_view.h"

namespace rc {

class SplashScreen : public Screen {
 public:
  void enter(const std::string&) override;
  void onKey(const KeyEvent&) override {}
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
  bool hasOverlay() const override { return confirm_; }
  void closeOverlays() override { confirm_ = false; }

 private:
  ListView menu_, confirmMenu_;
  bool confirm_ = false;
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

class ComingSoonScreen : public Screen {
 public:
  void enter(const std::string& name) override { name_ = name; }
  void onKey(const KeyEvent&) override;
  void render(Grid&, const Theme&, int64_t nowMs) override;

 private:
  std::string name_;
};

}  // namespace rc
