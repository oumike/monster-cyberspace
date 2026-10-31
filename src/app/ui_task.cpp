#include "app/ui_task.h"

#include <Arduino.h>

#include <algorithm>

#include "app/app_state.h"
#include "app/queues.h"
#include "chat/chat_source.h"
#include "hal/board.h"
#include "input/backspace.h"
#include "input/double_tap.h"
#include "input/gestures.h"
#include "input/key_event.h"
#include "input/touch_resolve.h"
#include "screens/composer.h"
#include "screens/draw.h"
#include "screens/router.h"
#include "store/locked_chat.h"
#include "ui/kb_light.h"
#include "ui/notify_light.h"
#include "ui/renderer.h"
#include "ui/screenshot.h"
#include <vector>
#include "ui/room_layout.h"
#include "ui/rotation.h"
#include "ui/screen_timeout.h"
#include "ui/splash_layout.h"
#include "ui/tap_map.h"
#include "ui/theme.h"
#include "ui/widgets/banner.h"
#include "ui/widgets/osk.h"
#include "ui/widgets/overlay.h"
#include "ui/widgets/symbol_table.h"
#include "util/utf8.h"

namespace rc::ui {

namespace {

constexpr uint32_t kFrameMs = 33;
constexpr int64_t kSaverMs = 10000;
constexpr int64_t kActivityMs = 5000;
constexpr int64_t kLockQuietMs = 3000;  // input ignored just after the screen locks
constexpr int64_t kBounceStepMs = 250;
constexpr size_t kMaxMentionNotes = 50;  // the oldest go first

// On; Dark (screen off); Saver, the 10 s peek at recent messages; Bounce, the
// screensaver the screen timeout locks into.
enum class Power { On, Dark, Saver, Bounce };

Renderer renderer;
Grid grid(1, 1);
Banner topBanner;
Gestures gestures;
DoubleTap doubleTap;  // wakes boards with doubleTapWakes
// Set by a touch that woke the screen: that finger is ignored until it lifts.
bool touchSwallow = false;
// Touch-only boards show no cursor, except on what a long press opened a
// menu for, until that menu (and any it leads to) closes.
bool baseCursor = false;
// The emoji tray opened from the on-screen keyboard takes the keyboard's
// place at the bottom; the keyboard comes back when the tray closes.
bool oskBehindTray = false;
bool pressMenu = false;
bool touchKey = false;  // ui::fromTouch()
bool oskWanted = false;  // ui::showKeyboard()
// The last Backspace that deleted text (backspaceIsBack boards).
uint32_t lastDeleteMs = 0;
bool deletedOnce = false;
TouchResolver resolver;
TapMap taps;
Osk osk;
Grid oskGrid(1, 1);
bool oskShown = false;
SymbolTable symbols;
Power power = Power::On;
int64_t saverUntil = 0, lastActivitySent = -kActivityMs;
int64_t lastInputMs = 0, lockedAt = INT64_MIN / 2, nextBounce = 0;
Power lockedTo = Power::Dark;  // where a peek returns to
uint8_t lit = 255;             // the backlight level set while the screen is lit
Bouncer bouncer;
// The monster's word on the screensaver, a new one every kSayMs.
MonsterWords words;
const char* saying = "";
int64_t sayUntil = 0;
constexpr int64_t kSayMs = 20000;
bool needResize = true;
// The text size the grid was last sized for: the saved one, or Compact on a
// screen that asks for it.
TextSize gridSize = TextSize::Comfortable;

// The splash and the monster screensaver look the same at every text size.
bool fixedSize() {
  return router::current() == router::Id::Splash || power == Power::Bounce || power == Power::Dark;
}

TextSize wantedSize() {
  if (fixedSize()) return TextSize::Comfortable;
  return router::screen().compactText() ? TextSize::Compact : app().cfg.textSize;
}

int64_t nowMs() { return int64_t(millis()); }

int oskOffsetPx = 0;  // the header's extra height, which moves the keyboard down

void resizeGrid() {
  int cols, rows;
  const TextSize ts = gridSize = wantedSize();
  const int rowH = cellSize(ts).h;
  // Touch boards without a keyboard or without keys that move a selection get
  // a taller status bar, easier to tap.
  const HeaderSize hd = !fixedSize() && board::hasTouch() &&
                                (!board::hasKeyboard() || !board::hasKeys())
                            ? touchHeader(board::panelWidth(), ts)
                            : HeaderSize{};
  gridDims(board::panelWidth(), board::panelHeight(), ts, cols, rows, hd.px);
  gestures.setWidth(board::panelWidth());
  oskOffsetPx = hd.px > 0 ? std::max(0, hd.px - rowH) : 0;
  const int slack = std::max(0, board::panelHeight() - rows * rowH - oskOffsetPx);
  grid.setHeader(hd);
  if (oskShown) {
    const int k = Osk::rows(ts);
    grid.resize(cols, rows - k);
    grid.setBottomPad(0);
    oskGrid.resize(cols, Osk::gridRows());
    oskGrid.setBottomPad(slack);
    osk.layout(cols, ts);
  } else {
    grid.resize(cols, rows);
    grid.setBottomPad(slack);
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
    o.size = gridSize;
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

void setLit(uint8_t level) {
  if (level == lit) return;
  lit = level;
  board::backlightLevel(level);
}

void drawBounce(const Theme& t);

// The keyboard light follows the setting while the screen is on.
void applyKbLight() {
  static int shownDuty = -1;
  static bool shownOn = false;
  if (!board::hasKbLight()) return;
  const int duty = kbLightDuty(app().cfg.kbLight);
  const bool on = power == Power::On;
  if (duty == shownDuty && on == shownOn) return;
  shownDuty = duty;
  shownOn = on;
  board::kbLight(uint8_t(duty), on);
}

// The new-message light: a double blink each second while something is
// unread, for the configured time after the last message.
NotifyBlinker blinker;
uint8_t notifyShown = 0;

void applyNotifyLight() {
  if (!board::hasNotifyLight()) return;
  uint8_t rgb = 0;
  if (blinker.active()) {
    const bool unread = !app().mentions.empty() || cmailUnread() > 0;
    rgb = blinker.tick(millis(), app().cfg.notifyLight, app().cfg.notifyTimeoutSecs, unread);
  }
  if (rgb == notifyShown) return;
  notifyShown = rgb;
  board::notifyLight(rgb);
}

void notifyMessage() {
  if (board::hasNotifyLight() && app().cfg.notifyLight != LedColor::Off) blinker.message(millis());
}

void goDark() {
  showOsk(false);
  power = lockedTo = Power::Dark;
  lit = 255;
  // An e-paper panel keeps its last image with the power off, so it is left
  // showing the still screensaver rather than a frozen screen.
  if (board::eink()) {
    sayUntil = 0;
    if (wantedSize() != gridSize) resizeGrid();
    drawBounce(currentTheme());
    renderer.draw(grid, gridSize);
  }
  board::displaySleep(true);
}

// The splash monster and name, bouncing slowly around a black screen. On
// e-paper it sits still in the middle.
void drawBounce(const Theme& t) {
  if (nowMs() >= sayUntil) {
    saying = words.next(esp_random());
    sayUntil = nowMs() + kSayMs;
  }
  std::vector<SplashLine> ls = splashLayout(grid.cols(), grid.rows(), "", saying);
  int minC = grid.cols(), minR = grid.rows(), maxC = 0, maxR = 0;
  ls.erase(std::remove_if(ls.begin(), ls.end(),
                          [](const SplashLine& l) {
                            return l.role == SplashRole::Detail || l.role == SplashRole::Prompt;
                          }),
           ls.end());
  for (const SplashLine& l : ls) {
    minC = std::min(minC, l.col);
    minR = std::min(minR, l.row);
    maxC = std::max(maxC, l.col + int(utf8ToU32(l.text).size()));
    maxR = std::max(maxR, l.row + 1);
  }
  grid.fill(t.bg);
  if (ls.empty()) return;
  const int w = maxC - minC, h = maxR - minR;
  const int maxX = std::max(0, grid.cols() - w), maxY = std::max(0, grid.rows() - h);
  if (board::eink()) bouncer = Bouncer{maxX / 2, maxY / 2, 0, 0};
  else bouncer.step(maxX, maxY);
  for (const SplashLine& l : ls)
    putLine(grid, bouncer.x + l.col - minC, bouncer.y + l.row - minR, l.text,
            l.role == SplashRole::Art ? t.selectBg : t.fg, t.bg);
}

// The screen timeout ran out: lock into the screensaver or screen off.
void lockScreen(int64_t now) {
  chatSource().command(Command{CmdType::ScreenOff});
  lockedAt = now;
  if (!app().cfg.screenSaver) return goDark();
  showOsk(false);
  power = lockedTo = Power::Bounce;
  sayUntil = 0;
  bouncer = Bouncer{int(random(0, 1000)), int(random(0, 1000)), random(0, 2) ? 1 : -1,
                    random(0, 2) ? 1 : -1};
  nextBounce = now;
  setLit(kSaverLevel);
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
        if (e.code == 0) app().mentions.clear();
        app().authNote.clear();
      } else {
        app().authNote = e.text;
      }
      if (e.ok && e.code == 0 &&
          (cur == Id::Home || cur == Id::RoomList || cur == Id::Room || cur == Id::CmailList ||
           cur == Id::Conversation || cur == Id::FeedList || cur == Id::Entry ||
           cur == Id::Composer || cur == Id::Watched || cur == Id::Bookmarks || cur == Id::Settings ||
           cur == Id::GuildList || cur == Id::Guild || cur == Id::Members ||
           cur == Id::Profile || cur == Id::UserPosts ||
           cur == Id::Notifications))
        router::go(Id::Setup);
      if (e.ok && e.code != 0 && (cur == Id::Setup || cur == Id::LoginForm)) router::go(Id::Home);
      break;
    case EvtType::PrefsChanged: {
      TextSize ts = textSizeFromIndex(e.code);
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
    case EvtType::WifiState:
      // The network task saved or forgot a network; the screens list cfg.wifi.
      if (e.code >= 0) {
        Settings s;
        settings::load(s);
        app().cfg.wifi = std::move(s.wifi);
      }
      break;
    case EvtType::AllRead:
      if (e.ok) app().mentions.clear();
      banner(e.text, !e.ok);
      break;
    case EvtType::MentionAlert: {
      // Not for the room on screen, nor again when the network side forgets
      // what it has seen and re-alerts.
      auto& ms = app().mentions;
      const int64_t at = e.at ? e.at : nowEpochMs();
      const bool open = cur == Id::Room && lockedChat().with([&](ChatState& s) {
        return !s.dm && s.roomSlug == e.arg;
      });
      const bool dup = std::any_of(ms.begin(), ms.end(), [&](const MentionNote& m) {
        return m.at == at && m.slug == e.arg && m.text == e.text;
      });
      if (!open && !dup) ms.push_back(MentionNote{e.arg, e.text, at});
      if (ms.size() > kMaxMentionNotes) ms.erase(ms.begin());
      notifyMessage();
      if (app().cfg.buzzer) board::beep(2000, 80);
      banner(e.text, false);
      break;
    }
    case EvtType::CmailAlert:
      // The unread count lives in the conversation list (Home sums it).
      notifyMessage();
      if (app().cfg.buzzer) board::beep(1600, 80);
      banner(e.text, false);
      break;
    case EvtType::NotifyPreview:
      notifyLightPreview(ledColorFromIndex(e.code));
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

void wake() {
  bool wasDark = power == Power::Dark;
  power = Power::On;
  lastInputMs = nowMs();
  redrawAll();
  chatSource().command(Command{CmdType::ScreenOn});
  if (wasDark) board::displaySleep(false);
  setLit(255);
}

void handleKey(const KeyEvent& key) {
  int64_t now = nowMs();
  KeyEvent k = key;
  if (power != Power::On) {
    // Just after the timeout locks the screen, input is ignored.
    if (now - lockedAt < kLockQuietMs) return;
    // Only a centre hold wakes (any input, on boards that ask); only a centre
    // tap peeks. The waking press does nothing else.
    if (wakesScreen(k, board::anyInputWakes())) {
      wake();
    } else if (peeksScreen(k)) {
      saverUntil = now + kSaverMs;
      if (power != Power::Saver) {
        const bool wasDark = power == Power::Dark;
        power = Power::Saver;
        grid.fill(currentTheme().bg);
        drawSaver(currentTheme());
        renderer.draw(grid, gridSize);
        if (wasDark) board::displaySleep(false);
        setLit(255);
      }
    }
    return;
  }
  lastInputMs = now;
  setLit(255);
  if (now - lastActivitySent >= kActivityMs) {
    lastActivitySent = now;
    chatSource().command(Command{CmdType::Activity, "", "", nowEpochMs()});
  }
  if (k.action == Action::ScreenOff || k.action == Action::PowerTap) {
    chatSource().command(Command{CmdType::ScreenOff});
    goDark();
    return;
  }
  if (symbols.isOpen()) {
    SymbolTable::Out o = symbols.key(k);
    if (o.kind == SymbolTable::Kind::Insert) {
      KeyEvent c;
      c.action = Action::Char;
      c.ch = o.ch;
      router::screen().onKey(c);
    } else if (o.kind == SymbolTable::Kind::Commands) {
      router::screen().onKey(KeyEvent{Action::Palette});
    }
    if (o.kind != SymbolTable::Kind::PassThrough) return;
  }
  // The symbol and emoji keys open their pickers while typing, and are the
  // palette elsewhere.
  if (k.action == Action::Symbols || k.action == Action::Emoji) {
    if (router::screen().acceptsText()) {
      symbols.setBackspaceCloses(board::backspaceIsBack());
      if (k.action == Action::Symbols)
        symbols.open(router::screen().paletteLabel(), SymbolTable::Set::Symbols, board::upDownOnly());
      else
        symbols.open(U"", SymbolTable::Set::Emoji, board::upDownOnly());
      return;
    }
    if (k.modifier) return;
    k.action = Action::Palette;
  }
  if (k.action == Action::Backspace && board::backspaceIsBack()) {
    uint32_t ms = millis();
    switch (resolveBackspace(router::screen().consumesBackspace(),
                             deletedOnce ? ms - lastDeleteMs : UINT32_MAX)) {
      case BackspaceUse::Delete:
        lastDeleteMs = ms;
        deletedOnce = true;
        break;
      case BackspaceUse::Back:
        k.action = Action::Back;
        break;
      case BackspaceUse::Drop:
        return;
    }
  }
  if (router::handleGlobal(k)) return;
  router::screen().onKey(k);
}

// Answers the console's "screenshot" command (scripts/screenshot.py) with the
// frame as drawn; read only, it touches nothing else.
SerialCommand console;

void sendScreenshot() {
  const uint16_t* f = renderer.frame();
  const int w = renderer.width(), h = renderer.height();
  if (!f || w <= 0 || h <= 0) return;
  static std::vector<uint8_t> buf;
  buf.resize(static_cast<size_t>(w) * 3);
  Serial.printf("\n@@SHOT %d %d\n", w, h);
  uint32_t crc = 0;
  size_t total = 0;
  for (int y = 0; y < h; ++y) {
    const size_t n = rle565Row(f + static_cast<size_t>(y) * w, w, buf.data());
    Serial.write(buf.data(), n);
    crc = crc32Update(crc, buf.data(), n);
    total += n;
  }
  Serial.printf("\n@@END %u %08lx\n", static_cast<unsigned>(total), static_cast<unsigned long>(crc));
}

void pollConsole() {
  while (Serial.available() > 0) {
    switch (console.feed(static_cast<char>(Serial.read()))) {
      case SerialCommand::Screenshot: sendScreenshot(); break;
#if RC_FAKE_API
      // The demo build only: key presses for scripted screenshots.
      case SerialCommand::Key: handleKey(console.key()); break;
      case SerialCommand::Type: {
        std::u32string text = utf8ToU32(console.text());
        for (char32_t c : text) {
          KeyEvent k;
          k.action = Action::Char;
          k.ch = c;
          handleKey(k);
        }
        break;
      }
#endif
      default: break;
    }
  }
}

void taskMain(void*) {
  ThemeId shown = app().cfg.theme;
  for (;;) {
    uint32_t start = millis();
    router::Id beforeScreen = router::current();
    Power beforePower = power;
    // Touch first, so a gesture resolves against the map of the frame on
    // screen; then the buttons. Touch acts only while lit, and wakes a dark
    // screen only on boards where any input wakes (that finger then does
    // nothing until it lifts).
    if (power != Power::On && board::hasTouch() && board::anyInputWakes()) {
      int16_t tx, ty;
      const bool down = board::readTouch(tx, ty);
      const bool waking = board::doubleTapWakes() ? doubleTap.feed(down, millis()) : down;
      if (waking && nowMs() - lockedAt >= kLockQuietMs) {
        wake();
        touchSwallow = true;
        gestures.reset();
      }
    } else if (power == Power::On && board::hasTouch()) {
      int16_t tx, ty;
      bool down = board::readTouch(tx, ty);
      if (touchSwallow && !down) touchSwallow = false;
      if (down) {
        lastInputMs = nowMs();
        setLit(255);
      }
      // A key of the on-screen keyboard lights while a finger is on it.
      if (down && oskShown && !touchSwallow) {
        const Target h = taps.hit(tx, ty);
        if (h.kind == TargetKind::Osk) osk.touch(h.index, millis());
      }
      Gesture g;
      if (!touchSwallow && gestures.feed(down, tx, ty, millis(), g)) {
        TouchOutput out;
        resolver.resolve(g, taps, out);
        if (out.hideBanner) topBanner.hide();
        const bool menuBefore = Overlay::anyOpen();
        touchKey = true;
        for (const KeyEvent& k : out.keys) {
          handleKey(k);
          if (g.kind == GestureKind::LongPress && !menuBefore && Overlay::anyOpen())
            pressMenu = true;
          if (k.action == Action::Focus && power == Power::On && !board::hasKeyboard() &&
              router::screen().acceptsText())
            showOsk(true);
        }
        if (out.oskKey >= 0 && oskShown) {
          osk.touch(out.oskKey, millis());
          OskOutput o = osk.press(out.oskKey, millis());
          if (o.key.action != Action::None) handleKey(o.key);
          if (o.hide) showOsk(false);
          if (o.key.action == Action::Emoji && symbols.isOpen()) {
            showOsk(false);
            oskBehindTray = true;
          }
        }
        touchKey = false;
      }
    }
    board::textEntry(power == Power::On && router::screen().acceptsText());
    KeyEvent key;
    while (board::pollKey(key)) handleKey(key);
    applyKbLight();
    Event e;
    while (nextEvent(e)) handleEvent(e);
    applyNotifyLight();

    int64_t now = nowMs();
    if (power == Power::Saver && now >= saverUntil) {
      if (lockedTo == Power::Bounce) {
        power = Power::Bounce;
        sayUntil = 0;
        nextBounce = now;
        setLit(kSaverLevel);
      } else {
        goDark();
      }
    }
    // The screen timeout: dim for the last seconds, then lock. Incoming
    // messages neither wake the screen nor restart the timeout.
    if (power == Power::On) {
      switch (idlePhase(now - lastInputMs, app().cfg.screenOnSecs)) {
        case IdlePhase::Active: setLit(255); break;
        case IdlePhase::Dim: setLit(kDimLevel); break;
        case IdlePhase::Lock: lockScreen(now); break;
      }
    }
    // A gesture in progress is dropped whenever the screen or power state
    // changes under it (screen change, going dark, the saver, waking).
    if (router::current() != beforeScreen || power != beforePower) {
      gestures.reset();
      doubleTap.reset();
      resolver.reset();
      if (router::current() != beforeScreen) {
        showOsk(false);
        oskBehindTray = false;
      }
    }
    if (oskShown && !router::screen().acceptsText()) showOsk(false);
    if (oskWanted) {
      oskWanted = false;
      if (power == Power::On && board::hasTouch() && !board::hasKeyboard() && !symbols.isOpen() &&
          router::current() == beforeScreen && router::screen().acceptsText())
        showOsk(true);
    }
    if (oskBehindTray && !symbols.isOpen()) {
      oskBehindTray = false;
      if (power == Power::On && router::current() == beforeScreen && router::screen().acceptsText())
        showOsk(true);
    }
    if (symbols.isOpen() && (router::current() != beforeScreen || power != Power::On ||
                             !router::screen().acceptsText()))
      symbols.close();
    // Settings and web config change the theme between frames; repaint it all.
    if (app().cfg.theme != shown) {
      shown = app().cfg.theme;
      needResize = true;
    }
    if (wantedSize() != gridSize) needResize = true;
    if (needResize) resizeGrid();
    if (!Overlay::anyOpen() || router::current() != beforeScreen) pressMenu = false;
    setCursorShown(baseCursor || pressMenu);
    const Theme& t = currentTheme();
    if (power == Power::On) {
      taps.clear();
      taps.setOrigin(cellSize(gridSize), 0);
      router::screen().render(grid, t, now);
      symbols.render(grid, grid.rows() - 1, t);
      topBanner.render(grid, grid.rows() - 1, t, now);
      taps.mapRows(0, rowSpans(grid, cellSize(gridSize).h, bigCellSize(gridSize).h));
      renderer.draw(grid, gridSize);
      if (oskShown) {
        taps.setOrigin(cellSize(gridSize), grid.rows(), oskOffsetPx);
        osk.render(oskGrid, t, millis());
        taps.mapRows(grid.rows(), rowSpans(oskGrid, cellSize(gridSize).h, bigCellSize(gridSize).h));
        renderer.draw(oskGrid, gridSize, grid.rows(), oskOffsetPx);
      }
    } else if (power == Power::Saver) {
      drawSaver(t);
      renderer.draw(grid, gridSize);
    } else if (power == Power::Bounce && now >= nextBounce) {
      // E-paper draws it once.
      nextBounce = board::eink() ? INT64_MAX : now + kBounceStepMs;
      drawBounce(t);
      renderer.draw(grid, gridSize);
    }
    pollConsole();
    uint32_t spent = millis() - start;
    vTaskDelay(pdMS_TO_TICKS(spent < kFrameMs ? kFrameMs - spent : 1));
  }
}

}  // namespace

void start() {
  if (board::canRotate()) board::setRotation(app().cfg.rotation);
  if (!renderer.begin()) Serial.println("[ui] no memory for the frame buffer");
  setActiveTaps(board::hasTouch() ? &taps : nullptr);
  baseCursor = board::hasKeys() || board::sideNav();
  setCursorShown(baseCursor);
  resolver.setTouchOnly(!board::hasKeys());
  Overlay::setConfirmButtons(board::hasTouch() && !board::hasKeys());
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

std::u32string textSizeItem() { return U"Text size: " + textSizeLabel(nextTextSize(app().cfg.textSize)); }

void cycleTextSize() { setTextSize(nextTextSize(app().cfg.textSize)); }

void setRotation(uint8_t r) { applyRotation(r); }

bool fromTouch() { return touchKey; }

const char* monsterWord() { return words.next(esp_random()); }

void showKeyboard() { oskWanted = true; }

void banner(const std::string& text, bool error) {
  topBanner.show(utf8ToU32(text), nowMs(), error ? 6000 : 4000, error);
}

void redrawAll() { needResize = true; }

void notifyLightPreview(LedColor c) {
  if (board::hasNotifyLight()) blinker.preview(c, millis());
}

}  // namespace rc::ui
