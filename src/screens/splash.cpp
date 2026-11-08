#include "core_version.h"
#include "app/app_state.h"
#include "app/ui_task.h"
#include "hal/board.h"
#include "screens/draw.h"
#include "screens/lv_draw.h"
#include "screens/screens.h"
#include "ui/splash_layout.h"
#include "util/utf8.h"

// How many times bigger the monster may grow on a roomy grid (the X4).
#ifndef RC_SPLASH_SCALE
#define RC_SPLASH_SCALE 1
#endif

namespace rc {

namespace {
constexpr int64_t kShowMs = 2000;
constexpr int64_t kBlinkMs = 500;
}  // namespace

void SplashScreen::enter(const std::string&) {
  since_ = -1;
  say_ = ui::monsterWord();
}

void SplashScreen::onKey(const KeyEvent&) { router::go(router::bootTarget()); }

void SplashScreen::render(Grid& g, const Theme& t, int64_t nowMs) {
  if (since_ < 0) since_ = nowMs;
  g.fill(t.bg);
  std::string detail = std::string("v") + RC_VERSION + " \u00b7 " + board::name();
  for (const auto& l : splashLayout(g.cols(), g.rows(), detail, say_, RC_SPLASH_SCALE, &app().mascot.art)) {
    switch (l.role) {
      case SplashRole::Art:
        putLine(g, l.col, l.row, l.text, t.selectBg, t.bg);
        g.setRowScale(l.row, l.scale);
        break;
      case SplashRole::Bubble: putLine(g, l.col, l.row, l.text, t.fg, t.bg); break;
      case SplashRole::Title: putLine(g, l.col, l.row, l.text, t.fg, t.bg); break;
      case SplashRole::Detail: putLine(g, l.col, l.row, l.text, t.dim, t.bg); break;
      case SplashRole::Prompt: {
        int end = l.col + putLine(g, l.col, l.row, l.text, t.selectBg, t.bg);
        // E-paper: a still cursor, not a blink.
        if (board::eink() || (nowMs - since_) / kBlinkMs % 2 == 0)
          g.put(end, l.row, U' ', t.cursorFg, t.cursorBg);
        break;
      }
    }
  }
  if (nowMs - since_ > kShowMs) router::go(router::bootTarget());
}

void SplashScreen::build(lv_obj_t* root) {
  text_.create(root);
  sync(since_ < 0 ? 0 : since_);
}

void SplashScreen::update(int64_t nowMs) {
  if (since_ < 0) since_ = nowMs;
  if (gate_.due(text_.obj(), nowMs, {board::eink() || (nowMs - since_) / kBlinkMs % 2 == 0})) sync(nowMs);
  if (nowMs - since_ > kShowMs) router::go(router::bootTarget());
}

void SplashScreen::sync(int64_t nowMs) {
  const Theme& t = lvui::currentTheme();
  const int64_t since = since_ < 0 ? nowMs : since_;
  std::string detail = std::string("v") + RC_VERSION + " \u00b7 " + board::name();
  text_.begin();
  for (const auto& l :
       splashLayout(text_.cols(), text_.rows(), detail, say_, RC_SPLASH_SCALE, &app().mascot.art)) {
    switch (l.role) {
      case SplashRole::Art: text_.line(l.col, l.row, l.text, t.selectBg); break;
      case SplashRole::Bubble: text_.line(l.col, l.row, l.text, t.fg); break;
      case SplashRole::Title: text_.line(l.col, l.row, l.text, t.fg); break;
      case SplashRole::Detail: text_.line(l.col, l.row, l.text, t.dim); break;
      case SplashRole::Prompt: {
        const int end = l.col + text_.line(l.col, l.row, l.text, t.selectBg);
        // E-paper: a still cursor, not a blink.
        if (board::eink() || (nowMs - since) / kBlinkMs % 2 == 0)
          text_.filled(end, l.row, U" ", t.cursorFg, t.cursorBg);
        break;
      }
    }
  }
  text_.end();
}

}  // namespace rc
