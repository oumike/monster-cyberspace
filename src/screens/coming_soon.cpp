#include "screens/lv_draw.h"
#include "screens/screens.h"
#include "app/app_state.h"

namespace rc {

void ComingSoonScreen::onKey(const KeyEvent& k) {
  if (k.action == Action::Back || k.action == Action::Left || k.action == Action::Select)
    router::go(router::Id::Home);
}

void ComingSoonScreen::build(lv_obj_t* root) {
  v_.make(root);
  v_->text_.create(root);
  v_->status_.create(root);
  v_->hint_.create(root);
  sync();
}

void ComingSoonScreen::update(int64_t nowMs) {
  if (gate_.due(v_->text_.obj(), nowMs)) sync();
}

void ComingSoonScreen::sync() {
  const Theme& t = lvui::currentTheme();
  syncStatus(v_->status_, utf8ToU32(name_), -1);
  const int mid = v_->text_.rows() / 2 - 1;
  v_->text_.begin();
  v_->text_.line(2, mid, name_, t.selectBg);
  v_->text_.line(2, mid + 1, "coming in a later version", t.dim);
  v_->text_.end();
  syncHint(v_->hint_, U"back: home", U"\u2039 home");
}

}  // namespace rc
