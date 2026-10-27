#include "app/ui_task.h"

#include <Arduino.h>

#include "app/app_state.h"
#include "app/queues.h"
#include "chat/chat_source.h"
#include "hal/board.h"
#include "input/gestures.h"
#include "input/key_event.h"
#include "input/touch_resolve.h"
#include "screens/composer.h"
#include "screens/router.h"
#include "store/locked_chat.h"
#include "ui/renderer.h"
#include "ui/room_layout.h"
#include "ui/rotation.h"
#include "ui/tap_map.h"
#include "ui/theme.h"
#include "ui/widgets/banner.h"
#include "ui/widgets/osk.h"
#include "util/utf8.h"

namespace rc::ui {

namespace {

constexpr uint32_t kFrameMs = 33;
constexpr int64_t kSaverMs = 10000;
constexpr int64_t kActivityMs = 5000;

enum class Power { On, Dark, Saver };

Renderer renderer;
Grid grid(1, 1);
Banner topBanner;
Gestures gestures;
TouchResolver resolver;
TapMap taps;
Osk osk;
Grid oskGrid(1, 1);
bool oskShown = false;
Power power = Power::On;
int64_t saverUntil = 0, lastActivitySent = -kActivityMs;
bool needResize = true;

int64_t nowMs() { return int64_t(millis()); }

void resizeGrid() {
  int cols, rows;
  const TextSize ts = app().cfg.textSize;
  gridDims(board::panelWidth(), board::panelHeight(), ts, cols, rows);
  if (oskShown) {
    const int k = Osk::rows(ts);
    grid.resize(cols, rows - k);
    oskGrid.resize(cols, k);
    osk.layout(cols, ts);
  } else {
    grid.resize(cols, rows);
  }
  needResize = false;
}

// The on-screen keyboard docks below the screen while a text field has focus.
void showOsk(bool on) {
  if (on == oskShown) return;
  oskShown = on;
  if (on) osk.reset();
  needResize = true;
}

// The screensaver: the newest messages of the current room, read-only.
void drawSaver(const Theme& t) {
  grid.fill(t.bg);
  std::string room;
  std::vector<DisplayLine> lines;
  lockedChat().with([&](ChatState& s) {
    room = s.roomSlug;
    std::deque<CircMessage> last;
    const auto& all = s.store.messages();
    size_t from = all.size() > 12 ? all.size() - 12 : 0;
    last.assign(all.begin() + from, all.end());
    LayoutOptions o;
    o.size = app().cfg.textSize;
    o.cols = grid.cols();
    o.selfUsername = s.self;
    o.revealed = &s.revealed;
    o.theme = &t;
    lines = layoutRoom(last, o);
  });
  std::u32string head = utf8ToU32(room.empty() ? std::string("no room open") : "#" + room);
  grid.putText(0, 0, head, t.dim, t.bg, grid.cols());
  std::u32string clock = utf8ToU32(clockText());
  grid.putText(grid.cols() - int(clock.size()), 0, clock, t.dim, t.bg, int(clock.size()));
  int bottom = grid.rows() - 1;  // the last row holds the hint
  int row = bottom - 1;
  for (int i = int(lines.size()) - 1; i >= 0 && row >= 1; --i, --row) {
    int col = 0;
    for (const Span& sp : lines[i].spans)
      col += grid.putText(col, row, sp.text, sp.fg, t.bg, grid.cols() - col);
  }
  grid.putText(0, bottom, utf8ToU32(board::wakeHint()), t.dim, t.bg, grid.cols());
}

void applyRotation(uint8_t r) {
  if (!board::canRotate()) return;
  app().cfg.rotation = rotationFromIndex(r);
  settings::saveUiPrefs(app().cfg);
  board::setRotation(app().cfg.rotation);
  renderer.resize();
  gestures.reset();
  resolver.reset();
  needResize = true;
}

void goDark() {
  showOsk(false);
  power = Power::Dark;
  board::displaySleep(true);
}

void handleEvent(const Event& e) {
  using router::Id;
  Id cur = router::current();
  switch (e.type) {
    case EvtType::ConnState:
      app().conn = e.code;
      break;
    case EvtType::AuthState:
      if (e.ok) {
        app().auth = e.code;
        app().user = e.code == 0 ? std::string() : e.text;
        app().authNote.clear();
      } else {
        app().authNote = e.text;
      }
      if (e.ok && e.code == 0 &&
          (cur == Id::Home || cur == Id::RoomList || cur == Id::Room || cur == Id::CmailList ||
           cur == Id::Conversation || cur == Id::FeedList || cur == Id::Entry ||
           cur == Id::Composer || cur == Id::Watched || cur == Id::Bookmarks || cur == Id::Settings))
        router::go(Id::Setup);
      if (e.ok && e.code != 0 && (cur == Id::Setup || cur == Id::LoginForm)) router::go(Id::Home);
      break;
    case EvtType::PrefsChanged: {
      TextSize ts = e.code == 0 ? TextSize::Comfortable : TextSize::Compact;
      app().cfg.buzzer = e.ok;
      parseThemeId(e.text, app().cfg.theme);
      if (ts != app().cfg.textSize) setTextSize(ts);  // saves too
      else settings::saveUiPrefs(app().cfg);
      break;
    }
    case EvtType::RotationChanged:
      applyRotation(uint8_t(e.code));
      break;
    case EvtType::Banner:
      banner(e.text, !e.ok);
      break;
    case EvtType::MentionAlert:
      ++app().unreadMentions;
      if (app().cfg.buzzer) board::beep(2000, 80);
      banner(e.text, false);
      break;
    case EvtType::CmailAlert:
      // The unread count lives in the conversation list (Home sums it).
      if (app().cfg.buzzer) board::beep(1600, 80);
      banner(e.text, false);
      break;
    case EvtType::FeedAlert:
      // code 1 asks for a beep; post_cooldown (code 0) is a banner only.
      if (e.code == 1 && app().cfg.buzzer) board::beep(1800, 80);
      banner(e.text, false);
      break;
    default:
      break;
  }
  // A post that settles after the Composer was left still clears its draft.
  if (cur != Id::Composer) composerBackgroundEvent(e);
  router::screen().onEvent(e);
}

void handleKey(const KeyEvent& k) {
  int64_t now = nowMs();
  if (power != Power::On) {
    // Only a centre hold wakes; only a centre tap peeks. Nothing else acts.
    if (wakesScreen(k)) {
      bool wasDark = power == Power::Dark;
      power = Power::On;
      redrawAll();
      chatSource().command(Command{CmdType::ScreenOn});
      if (wasDark) board::displaySleep(false);
    } else if (peeksScreen(k)) {
      saverUntil = now + kSaverMs;
      if (power == Power::Dark) {
        power = Power::Saver;
        grid.fill(currentTheme().bg);
        drawSaver(currentTheme());
        renderer.draw(grid, app().cfg.textSize);
        board::displaySleep(false);
      }
    }
    return;
  }
  if (now - lastActivitySent >= kActivityMs) {
    lastActivitySent = now;
    chatSource().command(Command{CmdType::Activity, "", "", nowEpochMs()});
  }
  if (k.action == Action::ScreenOff || k.action == Action::PowerTap) {
    chatSource().command(Command{CmdType::ScreenOff});
    goDark();
    return;
  }
  if (router::handleGlobal(k)) return;
  router::screen().onKey(k);
}

void taskMain(void*) {
  ThemeId shown = app().cfg.theme;
  for (;;) {
    uint32_t start = millis();
    router::Id beforeScreen = router::current();
    Power beforePower = power;
    // Touch first, so a gesture resolves against the map of the frame on
    // screen; then the buttons. Only while lit: touch never wakes a dark
    // screen.
    if (power == Power::On && board::hasTouch()) {
      int16_t tx, ty;
      bool down = board::readTouch(tx, ty);
      Gesture g;
      if (gestures.feed(down, tx, ty, millis(), g)) {
        TouchOutput out;
        resolver.resolve(g, taps, out);
        if (out.hideBanner) topBanner.hide();
        for (const KeyEvent& k : out.keys) {
          handleKey(k);
          if (k.action == Action::Focus && power == Power::On &&
              router::screen().acceptsText())
            showOsk(true);
        }
        if (out.oskKey >= 0 && oskShown) {
          OskOutput o = osk.press(out.oskKey, millis());
          if (o.key.action != Action::None) handleKey(o.key);
          if (o.hide) showOsk(false);
        }
      }
    }
    KeyEvent key;
    while (board::pollKey(key)) handleKey(key);
    Event e;
    while (nextEvent(e)) handleEvent(e);

    int64_t now = nowMs();
    if (power == Power::Saver && now >= saverUntil) goDark();
    // A gesture in progress is dropped whenever the screen or power state
    // changes under it (screen change, going dark, the saver, waking).
    if (router::current() != beforeScreen || power != beforePower) {
      gestures.reset();
      resolver.reset();
      if (router::current() != beforeScreen) showOsk(false);
    }
    if (oskShown && !router::screen().acceptsText()) showOsk(false);
    // Settings and web config change the theme between frames; repaint it all.
    if (app().cfg.theme != shown) {
      shown = app().cfg.theme;
      needResize = true;
    }
    if (needResize) resizeGrid();
    const Theme& t = currentTheme();
    if (power == Power::On) {
      taps.clear();
      taps.setOrigin(cellSize(app().cfg.textSize), 0);
      router::screen().render(grid, t, now);
      topBanner.render(grid, grid.rows() - 1, t, now);
      taps.mapRows(0, rowSpans(grid, cellSize(app().cfg.textSize).h,
                               bigCellSize(app().cfg.textSize).h));
      renderer.draw(grid, app().cfg.textSize);
      if (oskShown) {
        taps.setOrigin(cellSize(app().cfg.textSize), grid.rows());
        osk.render(oskGrid, t);
        renderer.draw(oskGrid, app().cfg.textSize, grid.rows());
      }
    } else if (power == Power::Saver) {
      drawSaver(t);
      renderer.draw(grid, app().cfg.textSize);
    }
    uint32_t spent = millis() - start;
    vTaskDelay(pdMS_TO_TICKS(spent < kFrameMs ? kFrameMs - spent : 1));
  }
}

}  // namespace

void start() {
  if (board::canRotate()) board::setRotation(app().cfg.rotation);
  if (!renderer.begin()) Serial.println("[ui] no memory for the frame buffer");
  setActiveTaps(board::hasTouch() ? &taps : nullptr);
  resizeGrid();
  router::go(router::Id::Splash);
  board::backlight(true);
  xTaskCreatePinnedToCore(taskMain, "ui", 16384, nullptr, 2, nullptr, 1);
}

void setTextSize(TextSize s) {
  app().cfg.textSize = s;
  settings::saveUiPrefs(app().cfg);
  needResize = true;
}

void setRotation(uint8_t r) { applyRotation(r); }

void banner(const std::string& text, bool error) {
  topBanner.show(utf8ToU32(text), nowMs(), error ? 6000 : 4000, error);
}

void redrawAll() { needResize = true; }

}  // namespace rc::ui
