#pragma once

#include <string>
#include <vector>

#include "lvui/tree_views.h"
#include "lvui/views/button_rows.h"
#include "lvui/views/cell_text.h"
#include "lvui/views/input_field.h"
#include "lvui/views/menu_overlay.h"
#include "lvui/views/text_box.h"
#include "lvui/views/top_bar.h"
#include "screens/lv_draw.h"
#include "screens/router.h"
#include "ui/widgets/input_line.h"
#include "ui/widgets/list_view.h"
#include "ui/widgets/overlay.h"
#include "ui/widgets/spell_strip.h"
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
  void build(lv_obj_t* root) override;
  void update(int64_t nowMs) override;
  bool hasOverlay() const override { return menu_ != Menu::None; }
  void closeOverlays() override { menu_ = Menu::None; box_.close(); }
  bool acceptsText() const override;
  std::u32string paletteLabel() const override { return U"post or discard\u2026"; }
  bool consumesBackspace() const override;

 private:
  enum Field { kTitle, kBody, kTopics, kPublic, kNsfw, kCw, kFieldCount };
  enum class Menu { None, Actions, ConfirmPost, ConfirmDiscard };

  void fieldKey(const KeyEvent&);
  void menuKey(const KeyEvent&);
  void openMenu(Menu m, std::u32string title, std::vector<ListItem> items);
  void pick();
  void post();
  void askLeave(bool toMap, bool toFeed);  // Back, the Feed key (toFeed), or the Map key (toMap)
  void goBack();        // the Feed, the guild, or the guild list
  void discard();       // clears the draft and goes back
  void load();          // the widgets from the draft
  void save();          // the focused field into the draft
  InputLine* line();    // the focused one-line field, or null

  // Where sync puts the fields, from the screen's rows.
  struct Rows {
    int cw, check, topics, title, label, bodyTop, bodyBottom;
    int pad;  // touch boards pad the title with a row above and below
  };
  static Rows layout(int rows, bool small, bool guild);
  std::u32string statusTitle(bool guild) const;  // takes the feed lock for a guild
  void hints(bool guild, const char32_t*& hint, const char32_t*& touch) const;
  void sync();

  InputLine title_, topics_, cw_;
  TextArea body_{4096};
  SpellStrip spell_;    // for the title and the body
  int focus_ = kTitle;
  int cols_ = 40;       // the body width, for moving by line
  Menu menu_ = Menu::None;
  bool toMap_ = false;  // the pending leave goes to the guild list
  bool toFeed_ = false;  // ... or to the feed list, even from a guild thread
  Overlay box_;

  // A touch target on the title's padding and the
  // checkboxes: a tap (or long press) emits {Focus, index, field}.
  struct FieldTap final : lvui::Tappable {
    void tapped(lv_obj_t* obj, bool hold) override;
    uint8_t field = 0;
    int16_t index = 0;
  };

  // LVGL views.
  lv_obj_t* root_ = nullptr;
  lv_obj_t* padAbove_ = nullptr;  // the title's padding rows, on inputBg
  lv_obj_t* padBelow_ = nullptr;
  lv_obj_t* promptTap_ = nullptr;  // over "Title ", which the padding's target covers
  lv_obj_t* checkFocus_ = nullptr;  // an outlined theme's focused checkbox
  lv_obj_t* publicTap_ = nullptr;
  lv_obj_t* nsfwTap_ = nullptr;
  FieldTap titleTap_, publicField_, nsfwField_;
  // The views, on the heap only while their tree is up (lvui/tree_views.h).
  struct Views {
    SyncGate<3> gate_;  // the update check, fresh with each tree
    lvui::TopBar status_;
    lvui::CellText text_;          // the Body label, its count and the checkboxes
    lvui::TextBox bodyView_;
    lvui::InputField titleView_, topicsView_, cwView_;
    lvui::SpellBar spellView_;
    lvui::HintBar hint_;
    lvui::MenuOverlay menuView_;
  };
  lvui::TreeViews<Views> v_;
};

// Settles a post that finished while another screen was showing: a success
// clears the draft, a failure shows its banner and keeps it.
void composerBackgroundEvent(const Event&);

}  // namespace rc
