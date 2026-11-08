#include "screens/draw.h"
#include "screens/lv_draw.h"
#include "screens/screens.h"
#include "app/app_state.h"

namespace rc {

void ComingSoonScreen::onKey(const KeyEvent& k) {
  if (k.action == Action::Back || k.action == Action::Left || k.action == Action::Select)
    router::go(router::Id::Home);
}

void ComingSoonScreen::render(Grid& g, const Theme& t, int64_t) {
  g.fill(t.bg);
  drawStatus(g, t, utf8ToU32(name_), -1);
  int mid = g.rows() / 2 - 1;
  putLine(g, 2, mid, name_, t.selectBg, t.bg);
  putLine(g, 2, mid + 1, "coming in a later version", t.dim, t.bg);
  drawHint(g, t, U"back: home", U"\u2039 home");
}

void ComingSoonScreen::build(lv_obj_t* root) {
  text_.create(root);
  status_.create(root);
  hint_.create(root);
  sync();
}

void ComingSoonScreen::update(int64_t) { sync(); }

void ComingSoonScreen::sync() {
  const Theme& t = lvui::currentTheme();
  syncStatus(status_, utf8ToU32(name_), -1);
  const int mid = text_.rows() / 2 - 1;
  text_.begin();
  text_.line(2, mid, name_, t.selectBg);
  text_.line(2, mid + 1, "coming in a later version", t.dim);
  text_.end();
  syncHint(hint_, U"back: home", U"\u2039 home");
}

}  // namespace rc
