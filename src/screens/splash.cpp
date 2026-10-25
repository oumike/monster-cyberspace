#include "core_version.h"
#include "hal/board.h"
#include "screens/draw.h"
#include "screens/screens.h"

namespace rc {

void SplashScreen::enter(const std::string&) { since_ = -1; }

void SplashScreen::render(Grid& g, const Theme& t, int64_t nowMs) {
  if (since_ < 0) since_ = nowMs;
  g.fill(t.bg);
  int mid = g.rows() / 2 - 1;
  putLine(g, 2, mid, "rhino-cyberspace", t.selectBg, t.bg);
  putLine(g, 2, mid + 1, std::string("v") + RC_VERSION + " \u00b7 " + board::name(), t.dim, t.bg);
  putLine(g, 2, mid + 3, "connecting\u2026", t.fg, t.bg);
  if (nowMs - since_ > 1200) router::go(router::bootTarget());
}

}  // namespace rc
