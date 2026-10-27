#pragma once

#include <string>
#include <vector>

#include "screens/router.h"
#include "ui/widgets/input_line.h"
#include "ui/widgets/list_view.h"
#include "ui/widgets/overlay.h"
#include "ui/widgets/text_area.h"

namespace rc {

// The entry Composer (spec §5): Title, Body, Topics, Public, NSFW and CW.
// Ctrl opens Post / Discard. The draft lives in RAM across visits until it
// is posted or discarded.
class ComposerScreen : public Screen {
 public:
  void enter(const std::string&) override;
  void onKey(const KeyEvent&) override;
  void onEvent(const Event&) override;
  void render(Grid&, const Theme&, int64_t nowMs) override;
  bool hasOverlay() const override { return menu_ != Menu::None; }
  void closeOverlays() override { menu_ = Menu::None; box_.close(); }
  bool acceptsText() const override;

 private:
  enum Field { kTitle, kBody, kTopics, kPublic, kNsfw, kCw, kFieldCount };
  enum class Menu { None, Actions, ConfirmPost, ConfirmDiscard };

  void fieldKey(const KeyEvent&);
  void menuKey(const KeyEvent&);
  void openMenu(Menu m, std::u32string title, std::vector<ListItem> items);
  void pick();
  void post();
  void askLeave();      // Back or the Feed key
  void discard();       // clears the draft and goes to the Feed
  void load();          // the widgets from the draft
  void save();          // the focused field into the draft
  InputLine* line();    // the focused one-line field, or null

  InputLine title_, topics_, cw_;
  TextArea body_{4096};
  int focus_ = kTitle;
  int cols_ = 40;       // the body width, for moving by line
  Menu menu_ = Menu::None;
  Overlay box_;
};

// Settles a post that finished while another screen was showing: a success
// clears the draft, a failure shows its banner and keeps it.
void composerBackgroundEvent(const Event&);

}  // namespace rc
