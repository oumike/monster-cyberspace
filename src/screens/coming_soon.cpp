#include "screens/draw.h"
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
  drawHint(g, t, U"back: home");
}

}  // namespace rc
