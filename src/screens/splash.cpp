#include "core_version.h"
#include "hal/board.h"
#include "screens/draw.h"
#include "screens/screens.h"
#include "ui/splash_layout.h"
#include "util/utf8.h"

namespace rc {

namespace {
constexpr int64_t kShowMs = 2000;
constexpr int64_t kBlinkMs = 500;
}  // namespace

void SplashScreen::enter(const std::string&) { since_ = -1; }

void SplashScreen::onKey(const KeyEvent&) { router::go(router::bootTarget()); }

void SplashScreen::render(Grid& g, const Theme& t, int64_t nowMs) {
  if (since_ < 0) since_ = nowMs;
  g.fill(t.bg);
  std::string detail = std::string("v") + RC_VERSION + " \u00b7 " + board::name();
  for (const auto& l : splashLayout(g.cols(), g.rows(), detail)) {
    switch (l.role) {
      case SplashRole::Art: putLine(g, l.col, l.row, l.text, t.selectBg, t.bg); break;
      case SplashRole::Title: putLine(g, l.col, l.row, l.text, t.fg, t.bg); break;
      case SplashRole::Detail: putLine(g, l.col, l.row, l.text, t.dim, t.bg); break;
      case SplashRole::Prompt: {
        int end = l.col + putLine(g, l.col, l.row, l.text, t.selectBg, t.bg);
        if ((nowMs - since_) / kBlinkMs % 2 == 0) g.put(end, l.row, U' ', t.cursorFg, t.cursorBg);
        break;
      }
    }
  }
  if (nowMs - since_ > kShowMs) router::go(router::bootTarget());
}

}  // namespace rc
