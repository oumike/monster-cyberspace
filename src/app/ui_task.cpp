#include "app/ui_task.h"

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <lvgl.h>

#include <algorithm>
#include <iterator>
#include <memory>
#include <mutex>

#include "app/app_state.h"
#include "app/power_service.h"
#include "app/queues.h"
#include "chat/chat_source.h"
#include "feature_flags.h"
#include "feed/compose.h"
#if RC_FEAT_GUILDS
#include "feed/feed_source.h"
#endif
#include "hal/board.h"
#include "lvui/cell_font.h"
#include "lvui/display.h"
#include "lvui/input.h"
#include "lvui/host.h"
#include "lvui/styles.h"
#include "lvui/views/banner.h"
#include "lvui/views/button_rows.h"
#include "lvui/views/cell_text.h"
#include "lvui/views/keyboard.h"
#include "lvui/views/menu_overlay.h"
#include "lvui/views/view_util.h"
#include "input/backspace.h"
#include "input/double_tap.h"
#include "input/function_keys.h"
#include "input/gestures.h"
#include "input/key_event.h"
#include "screens/composer.h"
#include "screens/router.h"
#include "screens/screens.h"
#include "store/locked_chat.h"
#include "chat/room_read.h"
#if RC_FEAT_ENTRIES
#include "store/locked_feed.h"
#endif
#if RC_FEAT_MASCOT
#include "store/mascot_store.h"
#endif
#include "ui/brightness.h"
#include "ui/haptics.h"
#include "ui/led_level.h"
#include "ui/kb_light.h"
#include "ui/lv/board_glue.h"
#include "ui/notify_light.h"
#include "ui/screenshot.h"
#include <vector>
#include "ui/room_layout.h"
#include "ui/rotation.h"
#include "ui/screen_timeout.h"
#include "ui/splash_layout.h"
#include "ui/theme.h"
#include "ui/wifi_light.h"
#include "ui/widgets/banner.h"
#include "ui/widgets/osk.h"
#include "ui/widgets/overlay.h"
#include "ui/widgets/canned_picker.h"
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
uint32_t inputCount = 0;  // ui::inputSeq()
bool oskWanted = false;  // ui::showKeyboard()
// The last Backspace that deleted text (backspaceIsBack boards).
uint32_t lastDeleteMs = 0;
bool deletedOnce = false;
Osk osk;
bool oskShown = false;
SymbolTable symbols;
// Boards that can't type (board::canType): canned replies instead of a text
// field. cannedEmoji: the emoji tray was opened from it, so a pick sends.
CannedPicker canned;
bool keysHelp = false;  // F10's table of the function keys
bool cannedEmoji = false;
// board::emojiKeyOpensActions: the emoji key opened the screen's actions
// while typing, on this screen; pressed again while they're open, the tray.
bool emojiOpenedActions = false;
router::Id emojiActionsScreen = router::Id::Splash;
Power power = Power::On;
int64_t saverUntil = 0, lastActivitySent = -kActivityMs;
int64_t lastInputMs = 0, lockedAt = INT64_MIN / 2, nextBounce = 0;
Power lockedTo = Power::Dark;  // where a peek returns to
uint8_t lit = 255;             // the backlight level set while the screen is lit
int sentBacklight = 255;       // what the board has: waking it sets full, sleeping 0
int brightnessPreview = -1;    // Settings' slider while it is open, else -1
Bouncer bouncer;
// The monster's word on the screensaver, a new one every kSayMs.
MonsterWords words;
std::string saying;
int64_t sayUntil = 0;
constexpr int64_t kSayMs = 20000;
// Set when the screen must be built again: the theme, text size, rotation
// or keyboard dock changed, or the screen woke.
bool needResize = true;
// The text size the root font was last set to: the saved one, or Compact on
// a screen that asks for it.
TextSize shownSize = TextSize::Comfortable;

TextSize wantedSize() {
  // A board with one text size uses it everywhere, splash and screensaver too.
  if (TextSize only; board::fixedTextSize(only)) return only;
  // The splash looks the same at every text size.
  if (router::current() == router::Id::Splash) return TextSize::Comfortable;
  const TextSize s = router::screen().compactText() ? TextSize::Compact : app().cfg.textSize;
#if RC_LOW_MEM
  // Without PSRAM a Compact grid on a big panel does not fit beside Wi-Fi.
  if (s == TextSize::Compact) return TextSize::Comfortable;
#endif
  return s;
}

// The taller touch status bar (lvui::Header), as the grid's touchHeader: on
// touch boards without both a keyboard and keys, except on the splash.
bool wantHeader() {
  return board::hasTouch() && (!board::hasKeyboard() || !board::hasKeys()) &&
         router::current() != router::Id::Splash;
}
// Whether the root and its layers were last laid out with the header.
bool shownHeader = false;

// The monster screensaver's size: the same at every text size.
TextSize bounceSize() {
  if (TextSize only; board::fixedTextSize(only)) return only;
  return TextSize::Comfortable;
}

int64_t nowMs() { return int64_t(millis()); }

// This frame's touch sample. LVGL's pointer and the gesture recogniser both
// read it, so they see the same finger; the last point stays on release.
struct TouchSample {
  bool down = false;
  int16_t x = 0, y = 0;
} touch;

// LVGL's pointer: the finger only while the screen is on and not swallowed.
bool readTouchCached(int16_t& x, int16_t& y) {
  x = touch.x;
  y = touch.y;
  return touch.down && power == Power::On && !touchSwallow;
}

// The keys LVGL's views emit while lv_timer_handler runs. They are handled
// once it returns: a key may change screens, and that deletes the tree the
// tap's event is still running in. held: the finger was still down (a long
// press), as the grid's LongPress.
struct TapKey {
  KeyEvent key;
  bool held;
};
std::vector<TapKey> tapKeys;
// An edge swipe acted this frame: LVGL's tap from the same finger is dropped.
bool dropTaps = false;

void queueKey(const KeyEvent& k) { tapKeys.push_back(TapKey{k, touch.down}); }

void showOsk(bool on);

// F10's table closes on a tap anywhere.
struct HelpTap : lvui::Tappable {
  void tapped(lv_obj_t*, bool) override { lvui::emit(KeyEvent{Action::KeysHelp}); }
} helpTap;

// F10's keys: a box over the area, and its text.
struct Help {
  lv_obj_t* box = nullptr;
  lvui::CellText text;
};

// What the UI task draws over every screen, on lv_layer_top(): in `area`
// (the screen above the keyboard) the emoji tray, the canned picker, F10's
// keys and the banner, in that order from the bottom (the canned picker
// comes to the front each time it opens); the docked keyboard below it; the
// screensaver over all; and the last image before sleep or power off.
// The tray, the picker, the keys and the keyboard exist only while they are
// open (syncLayers, relayout), for the memory they would hold.
struct Layers {
  lv_obj_t* root = nullptr;
  lv_obj_t* area = nullptr;
  std::unique_ptr<lvui::SymbolBox> symbols;
  std::unique_ptr<lvui::MenuOverlay> canned;
  std::unique_ptr<Help> help;
  lvui::Banner banner;
  lv_obj_t* oskBox = nullptr;
  std::unique_ptr<lvui::Keyboard> keyboard;
  lv_obj_t* saver = nullptr;
  lvui::CellText saverText;
  lv_obj_t* last = nullptr;
  lvui::CellText lastText;
  // The grid the area holds, for the parts made later.
  int cols = 1, rows = 1;
  lvui::Cell cell{};
};
std::unique_ptr<Layers> layers;
// What opening the tray, the picker or the keys needs free; less and it
// says "Out of memory" instead (lvui::ScreenHost::roomFor).
constexpr size_t kLayerBytes = 8192;

int displayW() { return int(lv_display_get_horizontal_resolution(lv_display_get_default())); }
int displayH() { return int(lv_display_get_vertical_resolution(lv_display_get_default())); }

// The function keys' table: a centred box over the area.
void buildHelp(Layers& L) {
  const int cols = L.cols, rows = L.rows;
  const lvui::Cell c = L.cell;
  const Theme& t = currentTheme();
  L.help = std::make_unique<Help>();
  Help& H = *L.help;
  H.box = lvui::plainBox(L.area);
  // Under the banner, as when everything was made at once.
  lv_obj_move_to_index(H.box, lv_obj_get_index(L.banner.obj()));
  lv_obj_set_size(H.box, lv_pct(100), lv_pct(100));
  lvui::addTap(H.box, &helpTap);
  size_t count = 0;
  const FunctionKey* keys = board::functionKeys(count);
  const int n = int(count);
  // The title and the keys, with a blank row above and below when they fit.
  const int content = n + 1, pad = rows >= content + 2 ? 1 : 0;
  const int w = std::max(1, std::min(cols - 2, 27)), h = std::max(1, std::min(rows, content + 2 * pad));
  lv_obj_t* frame = lvui::plainBox(H.box);
  lvui::addStyle(frame, lvui::Role::Screen);
  lv_obj_set_style_border_color(frame, lvui::color(t.dim), LV_PART_MAIN);
  lv_obj_set_style_border_width(frame, 1, LV_PART_MAIN);
  lv_obj_set_style_border_post(frame, true, LV_PART_MAIN);
  lvui::place(frame, std::max(0, (cols - w) / 2) * c.w, std::max(0, (rows - h) / 2) * c.h, w * c.w, h * c.h);
  H.text.create(frame);
  H.text.begin();
  H.text.line(2, pad, std::u32string_view(U"function keys"), t.fg, w - 4);
  for (int i = 0; i < n && pad + 1 + i < h; ++i) {
    H.text.line(2, pad + 1 + i, std::u32string_view(keys[i].key), t.selectBg, 5);
    H.text.line(8, pad + 1 + i, std::u32string_view(keys[i].label), t.fg, w - 10);
  }
  H.text.end();
}

// Sets the root font for size ts and builds the layers over the screen again
// for it (row heights are fixed when a view is made). The screen itself is
// built next (router::rebuild, or the go this runs inside).
void relayout(TextSize ts) {
  shownSize = ts;
  needResize = false;
  shownHeader = wantHeader();
  const lvui::Header hd = shownHeader ? lvui::touchHeader(ts) : lvui::Header{};
  lvui::setHeader(hd);
  const lv_font_t* f = lvui::font(ts);
  lv_obj_set_style_text_font(lv_screen_active(), f, LV_PART_MAIN);
  lv_obj_set_style_text_font(lv_layer_top(), f, LV_PART_MAIN);
  const lvui::Cell c = lvui::cellOf(lv_screen_active());
  const int w = displayW(), h = displayH();
  // The docked keyboard takes the bottom rows, right under the last screen
  // row (the rows' slack goes below it); screens fill what is left.
  const int kbRows = oskShown ? Osk::rows(ts) : 0;
  const int kbTop = oskShown ? hd.extra + std::max(0, (h - hd.extra) / std::max(1, c.h) - kbRows) * c.h : h;
  const int oskH = h - kbTop;
  lv_obj_set_style_pad_bottom(lv_screen_active(), oskH, LV_PART_MAIN);
  gestures.setWidth(board::panelWidth());
  const int cols = std::max(1, w / std::max(1, c.w)), rows = std::max(1, (h - oskH) / std::max(1, c.h));

  if (layers && layers->root) lv_obj_delete(layers->root);
  layers = std::make_unique<Layers>();
  Layers& L = *layers;
  L.cols = cols;
  L.rows = rows;
  L.cell = c;
  L.root = lvui::plainBox(lv_layer_top());
  lv_obj_set_size(L.root, w, h);
  L.area = lvui::plainBox(L.root);
  lv_obj_set_size(L.area, w, h - oskH);
  L.banner.create(L.area);
  L.banner.setOnHide([] { topBanner.hide(); });

  // Only while it shows: never on boards with keys.
  if (oskShown) {
    L.oskBox = lvui::plainBox(L.root);
    lv_obj_set_size(L.oskBox, w, std::max(1, std::min(oskH, kbRows * c.h)));
    lv_obj_set_pos(L.oskBox, 0, std::min(kbTop, h - 1));
    L.keyboard = std::make_unique<lvui::Keyboard>();
    L.keyboard->create(L.oskBox);
    L.keyboard->setOnHide([] { showOsk(false); });
    osk.layout(lvui::Keyboard::kLayoutCols, ts);
  }

  L.saver = lvui::plainBox(L.root);
  lv_obj_set_size(L.saver, w, h);
  lvui::addStyle(L.saver, lvui::Role::Screen);
  L.saverText.create(L.saver);
  lvui::setHidden(L.saver, true);

  L.last = lvui::plainBox(L.root);
  lv_obj_set_size(L.last, w, h);
  L.lastText.create(L.last);
  lvui::setHidden(L.last, true);
}

// router's hook before each build: the font the screen about to be built
// wants is set first.
void beforeBuild() {
  const TextSize ts = wantedSize();
  if (needResize || ts != shownSize || wantHeader() != shownHeader || !layers) relayout(ts);
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
  lvui::CellText& tx = layers->saverText;
  const int cols = tx.cols(), rows = tx.rows();
  std::string room;
  std::vector<DisplayLine> lines;
  lockedChat().with([&](ChatState& s) {
    room = s.roomSlug;
    std::deque<CircMessage> last;
    const auto& all = s.store.messages();
    size_t from = all.size() > 12 ? all.size() - 12 : 0;
    last.assign(all.begin() + from, all.end());
    LayoutOptions o;
    o.size = shownSize;
    o.cols = cols;
    o.selfUsername = s.self;
    o.revealed = &s.revealed;
    o.theme = &t;
    lines = layoutRoom(last, o);
  });
  tx.begin();
  std::u32string head = utf8ToU32(room.empty() ? std::string("no room open") : "#" + room);
  tx.line(0, 0, head, t.dim, cols);
  std::u32string clock = utf8ToU32(clockText());
  tx.line(cols - int(clock.size()), 0, clock, t.dim, int(clock.size()));
  int bottom = rows - 1;  // the last row holds the hint
  int row = bottom - 1;
  for (int i = int(lines.size()) - 1; i >= 0 && row >= 1; --i, --row) {
    int col = 0;
    for (const Span& sp : lines[i].spans) col += tx.line(col, row, sp.text, sp.fg, cols - col);
  }
  tx.line(0, bottom, utf8ToU32(board::wakeHint()), t.dim, cols);
  tx.end();
}

void applyRotation(uint8_t r) {
  if (!board::canRotate()) return;
  app().cfg.rotation = rotationFromIndex(r);
  settings::saveUiPrefs(app().cfg);
  board::setRotation(app().cfg.rotation);
  lvglResize();
  gestures.reset();
  needResize = true;
}

// The lit level at the brightness setting (or the slider's preview), sent
// when it differs from what the board has. Not while the panel sleeps.
void applyBacklight() {
  if (power == Power::Dark) return;
  const int pct = brightnessPreview > 0 ? brightnessPreview : app().cfg.brightness;
  const uint8_t v = backlightFor(lit, uint8_t(pct));
  if (v == sentBacklight) return;
  sentBacklight = v;
  board::backlightLevel(v);
}

void setLit(uint8_t level) {
  lit = level;
  applyBacklight();
}

// Wakes the panel, which comes back at the board's full level.
void panelOn() {
  board::displaySleep(false);
  sentBacklight = 255;
}

// The status LEDs' level and the power light follow their settings; the
// Wi-Fi light, checked each second, the connection, in its state's colour.
int ledLevelPreview = -1;  // Settings' slider while it is open, else -1

void applyStatusLights() {
  static int shownLevel = -1, shownPower = -1, shownWifi = -1;
  static uint32_t wifiCheckedAt = 0;
  const Settings& c = app().cfg;
  if (board::hasLedLevel()) {
    const LedLevel l = ledLevelPreview >= 0 ? ledLevelFromIndex(ledLevelPreview) : c.ledLevel;
    const int v = ledLevelValue(l);
    if (v != shownLevel) {
      shownLevel = v;
      board::ledLevel(uint8_t(v));
    }
  }
  if (board::hasPowerLight()) {
    const int rgb = ledColorRgb(c.powerLight);
    if (rgb != shownPower) {
      shownPower = rgb;
      board::powerLight(uint8_t(rgb));
    }
  }
  if (board::hasWifiLight() && millis() - wifiCheckedAt >= 1000) {
    wifiCheckedAt = millis();
    const WifiLight w = wifiLightState(wifiUp(), setupApUp());
    const LedColor col = w == WifiLight::Joined    ? c.wifiJoined
                         : w == WifiLight::SetupAp ? c.wifiSetup
                                                   : c.wifiLooking;
    const int rgb = ledColorRgb(col);
    if (rgb != shownWifi) {
      shownWifi = rgb;
      board::wifiLight(uint8_t(rgb));
    }
  }
}

void drawBounce(const Theme& t);
void syncSaver(int64_t now);

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

// The new-message light: a double blink per source (the last room, mentions,
// Feed, C-Mail) in its colour while it has something unread, for the
// configured time after its last message. A board with a light of its own
// for C-Mail or the Feed blinks that source there instead.
NotifyBlinker blinker, cmailBlinker, feedBlinker;
uint8_t notifyShown = 0, cmailShown = 0, feedShown = 0;

NotifyBlinker* blinkerFor(NotifySource s) {
  if (s == NotifySource::Cmail && board::hasCmailLight()) return &cmailBlinker;
  if (s == NotifySource::Feed && board::hasFeedLight()) return &feedBlinker;
  return board::hasNotifyLight() ? &blinker : nullptr;
}

void notifyMessage(NotifySource s) {
  if (NotifyBlinker* b = blinkerFor(s)) b->message(s, millis());
}

#if RC_FEAT_CIRC
// The last room opened: unread and not on screen. A new message there, or
// its turning unread, starts the light.
uint32_t channelCheckMs = 0;
bool channelUnread = false;
int64_t channelAt = 0;

bool lastRoomUnread(int64_t* at = nullptr) {
  const std::string& slug = app().cfg.lastRoom;
  if (power == Power::On && router::current() == router::Id::Room &&
      lockedChat().with([&](ChatState& s) { return !s.dm && s.roomSlug == slug; }))
    return false;
  return lockedChat().with([&](ChatState& s) {
    for (const RoomInfo& r : s.rooms)
      if (r.slug == slug) {
        if (at) *at = r.lastMessageAt;
        return roomUnread(r, s.readAt);
      }
    return false;
  });
}

void watchChannel() {
  const uint32_t now = millis();
  if (now - channelCheckMs < 250) return;
  channelCheckMs = now;
  int64_t at = channelAt;
  const bool unread = lastRoomUnread(&at);
  if (unread && (!channelUnread || at > channelAt)) notifyMessage(NotifySource::Channel);
  channelUnread = unread;
  channelAt = at;
}
#endif

void showLight(NotifyBlinker& b, const NotifyInputs* in, uint8_t& shown, void (*write)(uint8_t)) {
  const uint8_t rgb = in && b.active() ? b.tick(millis(), *in, app().cfg.notifyTimeoutSecs) : 0;
  if (rgb == shown) return;
  shown = rgb;
  write(rgb);
}

void applyNotifyLight() {
  const bool msg = board::hasNotifyLight(), mail = board::hasCmailLight(),
             feed = board::hasFeedLight();
  if (!msg && !mail && !feed) return;
#if RC_FEAT_CIRC
  if (msg) watchChannel();
#endif
  NotifyInputs in;
  const bool any = blinker.active() || cmailBlinker.active() || feedBlinker.active();
  if (any) {
    const Settings& c = app().cfg;
    in.color[int(NotifySource::Channel)] = c.notifyLight;
    in.color[int(NotifySource::Mention)] = c.notifyMention;
    in.color[int(NotifySource::Feed)] = c.notifyFeed;
    in.color[int(NotifySource::Cmail)] = c.notifyCmail;
#if RC_FEAT_CIRC
    in.unread[int(NotifySource::Channel)] = channelUnread;
#endif
    in.unread[int(NotifySource::Mention)] = !app().mentions.empty();
#if RC_FEAT_ENTRIES
    in.unread[int(NotifySource::Feed)] =
        lockedFeed().with([](FeedState& s) { return !s.notices.empty(); });
#endif
#if RC_FEAT_CMAIL
    in.unread[int(NotifySource::Cmail)] = cmailUnread() > 0;
#endif
  }
  const NotifyInputs* p = any ? &in : nullptr;
  if (msg) showLight(blinker, p, notifyShown, board::notifyLight);
  if (mail) showLight(cmailBlinker, p, cmailShown, board::cmailLight);
  if (feed) showLight(feedBlinker, p, feedShown, board::feedLight);
}

void goDark() {
  showOsk(false);
  power = lockedTo = Power::Dark;
  lit = 255;
  sentBacklight = 0;
  // An e-paper panel keeps its last image with the power off, so it is left
  // showing the still screensaver rather than a frozen screen.
  if (board::eink() && layers) {
    sayUntil = 0;
    if (layers->oskBox) lvui::setHidden(layers->oskBox, true);
    syncSaver(nowMs());
    drawBounce(currentTheme());
    lv_refr_now(nullptr);
  }
  board::displaySleep(true);
}

// The splash monster and name, bouncing slowly around a black screen. On
// e-paper it sits still in the middle.
void drawBounce(const Theme& t) {
  if (nowMs() >= sayUntil) {
    saying = words.next(esp_random(), app().mascot.phrases);
    sayUntil = nowMs() + kSayMs;
  }
  lvui::CellText& tx = layers->saverText;
  const int cols = tx.cols(), rows = tx.rows();
  std::vector<SplashLine> ls = splashLayout(cols, rows, "", saying, 1, &app().mascot.art);
  int minC = cols, minR = rows, maxC = 0, maxR = 0;
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
  tx.begin();
  if (!ls.empty()) {
    const int w = maxC - minC, h = maxR - minR;
    const int maxX = std::max(0, cols - w), maxY = std::max(0, rows - h);
    if (board::eink()) bouncer = Bouncer{maxX / 2, maxY / 2, 0, 0};
    else bouncer.step(maxX, maxY);
    for (const SplashLine& l : ls)
      tx.line(bouncer.x + l.col - minC, bouncer.y + l.row - minR, l.text,
              l.role == SplashRole::Art ? t.selectBg : t.fg);
  }
  tx.end();
}

// The screensaver layer: hidden while the screen is on; the peek's messages,
// or the monster (in its own size), each frame it is due.
void syncSaver(int64_t now) {
  Layers& L = *layers;
  const bool shown = power != Power::On;
  lvui::setHidden(L.saver, !shown);
  if (!shown) return;
  const lv_font_t* f = lvui::font(power == Power::Saver ? shownSize : bounceSize());
  if (lv_obj_get_style_text_font(L.saver, LV_PART_MAIN) != f)
    lv_obj_set_style_text_font(L.saver, f, LV_PART_MAIN);
  const Theme& t = currentTheme();
  if (power == Power::Saver) {
    drawSaver(t);
  } else if (power == Power::Bounce && now >= nextBounce) {
    // E-paper draws it once.
    nextBounce = board::eink() ? INT64_MAX : now + kBounceStepMs;
    drawBounce(t);
  }
}

// Deletes a part made while open, with its objects.
template <typename T>
void drop(std::unique_ptr<T>& part, lv_obj_t* obj) {
  if (!part) return;
  if (obj) lv_obj_delete(obj);
  part.reset();
}

// Everything over the screen, from its models. The tray, the picker and the
// keys are made as they open and deleted as they close; with too little
// memory to make one, it closes again and "Out of memory" shows.
void syncLayers(int64_t now) {
  Layers& L = *layers;
  const bool on = power == Power::On;
  if (symbols.isOpen() && !L.symbols) {
    if (lvui::ScreenHost::roomFor(kLayerBytes)) {
      L.symbols = std::make_unique<lvui::SymbolBox>();
      L.symbols->create(L.area);
      lv_obj_move_to_index(L.symbols->obj(), 0);  // the bottom of the area
    } else {
      symbols.close();
      cannedEmoji = false;
    }
  }
  if (L.symbols) {
    L.symbols->sync(symbols);
    if (!symbols.isOpen()) drop(L.symbols, L.symbols->obj());
  }
  if (canned.isOpen() && !L.canned) {
    if (lvui::ScreenHost::roomFor(kLayerBytes)) {
      L.canned = std::make_unique<lvui::MenuOverlay>();
      L.canned->create(L.area);
      L.canned->setPlace(OverlayPlace::Centre, (L.rows - 1) * L.cell.h);
    } else {
      canned.close();
    }
  }
  if (L.canned) {
    L.canned->sync(canned.box());
    if (!canned.isOpen()) drop(L.canned, L.canned->backdrop());
  }
  const bool helpShown = on && keysHelp;
  if (helpShown && !L.help) {
    if (lvui::ScreenHost::roomFor(kLayerBytes)) buildHelp(L);
    else keysHelp = false;
  } else if (!helpShown && L.help) {
    drop(L.help, L.help->box);
  }
  L.banner.sync(topBanner, now);
  if (L.oskBox) lvui::setHidden(L.oskBox, !(on && oskShown));
  if (on && oskShown && L.keyboard) L.keyboard->sync(osk, shownSize);
  syncSaver(now);
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

// Web compose waiting on the network (runWebCompose below).
#if RC_FEAT_CMAIL
std::string mailText;  // a new C-Mail's text, sent once its conversation opens
#endif
bool threadPosting = false;
void sendCanned(const std::u32string& text);

void handleEvent(const Event& e) {
  ++inputCount;
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
           cur == Id::Notifications || cur == Id::Topics || cur == Id::Topic ||
           cur == Id::Following))
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
    case EvtType::RoomsRead:
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
      if (!open) notifyMessage(NotifySource::Mention);
      if (app().cfg.buzzer) board::beep(2000, 80);
      if (app().cfg.alertVibrate && board::hasHaptic()) board::haptic(kAlertBuzzMs);
      banner(e.text, false);
      break;
    }
#if RC_FEAT_CMAIL
    case EvtType::CmailAlert:
      // The unread count lives in the conversation list (Home sums it).
      notifyMessage(NotifySource::Cmail);
      if (app().cfg.buzzer) board::beep(1600, 80);
      if (app().cfg.alertVibrate && board::hasHaptic()) board::haptic(kAlertBuzzMs);
      banner(e.text, false);
      break;
#endif
    case EvtType::NotifyPreview:
      notifyLightPreview(ledColorFromIndex(e.code & 0xff), NotifySource((e.code >> 8) & 0xff));
      break;
    case EvtType::PhrasesChanged:
      app().cfg.phrases = splitPhrases(e.text);
      break;
#if RC_FEAT_MASCOT
    case EvtType::MascotChanged:
      mascotstore::load(app().mascot);  // a failed read keeps the current one
      break;
#endif
#if RC_FEAT_ENTRIES
    case EvtType::FeedAlert:
      // code 1 asks for a beep; post_cooldown (code 0) is a banner only.
      if (e.code == 1 && app().cfg.buzzer) board::beep(1800, 80);
      if (e.code == 1 && app().cfg.alertVibrate && board::hasHaptic()) board::haptic(kAlertBuzzMs);
      if (e.code == 1) notifyMessage(NotifySource::Feed);
      banner(e.text, false);
      break;
#endif
#if RC_FEAT_CMAIL
    case EvtType::ConversationStarted:
      // A web-composed C-Mail: open the conversation and send the text there.
      if (!mailText.empty()) {
        std::string text = std::move(mailText);
        mailText.clear();
        if (!e.ok) {
          banner(e.text.empty() ? std::string("Couldn't start a C-Mail") : e.text, true);
        } else {
          router::go(Id::Conversation, e.text);
          // Out of memory, go stays where it was (and says so): the text
          // goes nowhere else.
          if (router::current() == Id::Conversation && router::hasTree() &&
              router::screen().acceptsText())
            sendCanned(utf8ToU32(text));
        }
        return;  // not the C-Mail list's, which didn't ask
      }
      break;
#endif
#if RC_FEAT_GUILDS
    case EvtType::EntryPosted:
      if (threadPosting) {
        threadPosting = false;
        banner(e.ok ? std::string("Thread posted")
                    : e.text.empty() ? std::string("Thread not posted") : e.text,
               !e.ok);
        return;
      }
      break;
#endif
    default:
      break;
  }
#if RC_FEAT_COMPOSER
  // A post that settles after the Composer was left still clears its draft.
  if (cur != Id::Composer) composerBackgroundEvent(e);
#endif
#if RC_FEAT_NOTES
  if (cur != Id::NoteEdit) noteBackgroundEvent(e);
#endif
  router::screen().onEvent(e);
}

void wake() {
  bool wasDark = power == Power::Dark;
  power = Power::On;
  lastInputMs = nowMs();
  redrawAll();
  chatSource().command(Command{CmdType::ScreenOn});
  if (wasDark) panelOn();
  setLit(255);
}

// The last image before deep sleep or power off; e-paper keeps it. whole:
// the line alone, centred; otherwise it replaces the bottom row of what is
// on screen (the open screen or the screensaver).
void finalScreen(const std::string& line, bool whole) {
  showOsk(false);
  const Theme& t = currentTheme();
  panelOn();
  Layers& L = *layers;
  if (L.oskBox) lvui::setHidden(L.oskBox, true);
  lvui::setHidden(L.last, false);
  if (whole) lvui::setBgColor(L.last, t.bg);
  else lv_obj_set_style_bg_opa(L.last, LV_OPA_TRANSP, LV_PART_MAIN);
  lvui::CellText& tx = L.lastText;
  const int cols = tx.cols(), rows = tx.rows();
  const std::u32string text = utf8ToU32(line);
  const int w = int(text.size()), c0 = std::max(0, (cols - w) / 2);
  tx.begin();
  if (whole) {
    tx.line(c0, rows / 2, text, t.fg);
  } else {
    std::u32string row(std::max(cols, 0), U' ');
    for (int i = 0; i < w && c0 + i < cols; ++i) row[c0 + i] = text[i];
    tx.filled(0, rows - 1, row, t.fg, t.bg);
  }
  tx.end();
  lv_refr_now(nullptr);
  board::displaySleep(true);
}

// Sleep or power off came back: the screen goes on as it was.
void hideFinal() {
  if (layers) lvui::setHidden(layers->last, true);
}

// A canned reply goes in as typing, then Select sends it through the
// screen's own send path.
// Web compose: one request waits in the slot until the UI task takes it.
std::mutex composeMu;
ComposeReq composeSlot;
bool composeWaiting = false;
ComposeView composeShown;
bool composeBusy = false;  // a C-Mail or thread from the page still settling

ComposeView currentComposeView() {
  ComposeView v;
  const router::Id id = router::current();
  v.takesText = router::screen().acceptsText();
  if (id == router::Id::Room || id == router::Id::Conversation) {
    lockedChat().with([&](ChatState& s) {
      if (!s.dm) {
        v.label = "#" + s.roomSlug;
        return;
      }
      for (const auto& c : s.conversations)
        if (c.id == s.roomSlug) v.label = "C-Mail with @" + c.partner;
      if (v.label.empty()) v.label = "a C-Mail conversation";
    });
  }
#if RC_FEAT_ENTRIES
  else if (id == router::Id::Entry) {
    v.label = "replies to the open post";
  }
#endif
  if (v.label.empty()) v.takesText = false;
  return v;
}

void sendCanned(const std::u32string& text);

void runWebCompose() {
  ComposeReq r;
  {
    std::lock_guard<std::mutex> l(composeMu);
    composeShown = currentComposeView();
#if RC_FEAT_CMAIL
    composeBusy = !mailText.empty() || threadPosting;
#else
    composeBusy = threadPosting;
#endif
    if (!composeWaiting) return;
    composeWaiting = false;
    ++inputCount;
    r = std::move(composeSlot);
    composeBusy = r.kind != ComposeKind::Open;
  }
  switch (r.kind) {
    case ComposeKind::Open:
      if (!router::hasTree()) {
        banner("Web compose: out of memory", true);
        break;
      }
      if (!router::screen().acceptsText()) {
        banner("Web compose: nothing open takes text", true);
        break;
      }
      canned.close();
      router::screen().onKey(KeyEvent{Action::ClearLine});
      sendCanned(utf8ToU32(r.text));
      break;
#if RC_FEAT_CMAIL
    case ComposeKind::Mail:
      mailText = std::move(r.text);
      chatSource().command(Command{CmdType::StartConversation, r.to});
      banner("Starting a C-Mail to @" + r.to + "\u2026", false);
      break;
#endif
#if RC_FEAT_GUILDS
    case ComposeKind::Thread: {
      EntryDraft d;
      d.title = r.title;
      d.body = r.text;
      threadPosting = true;
      feedSource().command(Command{CmdType::PostGuildThread, r.guild, guildThreadJson(d)});
      banner("Posting a thread in " + r.guild + "\u2026", false);
      break;
    }
#endif
    default:
      break;  // webCompose turned away a target not built
  }
}

void sendCanned(const std::u32string& text) {
  for (char32_t c : text) {
    KeyEvent k;
    k.action = Action::Char;
    k.ch = c;
    router::screen().onKey(k);
  }
  router::screen().onKey(KeyEvent{Action::Select});
}

void handleKey(const KeyEvent& key) {
  ++inputCount;
  int64_t now = nowMs();
  KeyEvent k = key;
  power_service::input();
  if (k.action == Action::PowerOff && board::sleepsOnBattery()) {
    finalScreen("Powered off \u2014 press Power to turn on", true);
    power_service::powerOff();
    return;
  }
  if (power != Power::On) {
    // Just after the timeout locks the screen, input is ignored.
    if (now - lockedAt < kLockQuietMs) return;
    // Only a centre hold wakes (any input, on boards that ask); only a centre
    // tap peeks (unless board::keysPeek is off). The waking press does
    // nothing else.
    if (wakesScreen(k, board::anyInputWakes())) {
      wake();
    } else if (board::keysPeek() && peeksScreen(k)) {
      saverUntil = now + kSaverMs;
      if (power != Power::Saver) {
        const bool wasDark = power == Power::Dark;
        power = Power::Saver;
        if (wasDark) panelOn();
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
  if (k.action == Action::KbLightStep) {
    if (board::hasKbLight()) {
      app().cfg.kbLight = nextKbLight(app().cfg.kbLight);
      settings::saveKbLight(app().cfg.kbLight);
      applyKbLight();
    }
    return;
  }
  // No tree (out of memory, boards without PSRAM): nothing on screen to act
  // on, so the key does nothing until router::update builds it again.
  if (!router::hasTree()) return;
  // The help closes on any key or tap; F10 again only closes it.
  if (keysHelp) {
    keysHelp = false;
    if (k.action == Action::KeysHelp || !isJump(k.action)) return;
  } else if (k.action == Action::KeysHelp) {
    keysHelp = true;
    return;
  }
  // Function-key jumps work from anywhere, past an open picker.
  if (isJump(k.action) && (canned.isOpen() || symbols.isOpen())) {
    canned.close();
    symbols.close();
    cannedEmoji = false;
  }
  if (canned.isOpen()) {
    CannedPicker::Out o = canned.key(k);
    if (o.kind == CannedPicker::Kind::Send) {
      sendCanned(o.text);
    } else if (o.kind == CannedPicker::Kind::Emoji) {
#if RC_FEAT_EMOJI
      cannedEmoji = true;
      symbols.setBackspaceCloses(false);
      symbols.open(U"", SymbolTable::Set::Emoji, board::upDownOnly());
#endif
    } else if (o.kind == CannedPicker::Kind::Closed) {
      router::screen().onKey(KeyEvent{Action::ClearLine});  // a reply's "@name "
    }
    return;
  }
  if (symbols.isOpen() && cannedEmoji) {
    SymbolTable::Out o = symbols.key(k);
    if (o.kind == SymbolTable::Kind::Insert) {
      symbols.close();
      sendCanned(std::u32string(1, o.ch));
    }
    if (!symbols.isOpen()) cannedEmoji = false;
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
      KeyEvent c{Action::Palette};
      if (router::screen().paletteTypesSlash()) {
        c.action = Action::Char;
        c.ch = U'/';
      }
      router::screen().onKey(c);
    }
    if (o.kind != SymbolTable::Kind::PassThrough) return;
  }
  // The symbol and emoji keys open their pickers while typing, and are the
  // palette elsewhere.
#if RC_FEAT_EMOJI
  if (k.action == Action::Emoji && board::emojiKeyOpensActions()) {
    Screen& s = router::screen();
    bool again = emojiOpenedActions && emojiActionsScreen == router::current() && s.hasOverlay();
    emojiOpenedActions = false;
    if (again) {
      s.closeOverlays();
      if (s.acceptsText()) {
        symbols.setBackspaceCloses(board::backspaceIsBack());
        symbols.open(U"", SymbolTable::Set::Emoji, board::upDownOnly());
      }
      return;
    }
    if (s.acceptsText()) {
      s.onKey(KeyEvent{Action::Palette});
      if (s.hasOverlay()) {
        emojiOpenedActions = true;
        emojiActionsScreen = router::current();
      } else if (s.acceptsText()) {
        // No actions here: straight to the tray.
        symbols.setBackspaceCloses(board::backspaceIsBack());
        symbols.open(U"", SymbolTable::Set::Emoji, board::upDownOnly());
      }
      return;
    }
  }
#endif
  if (k.action == Action::Symbols || k.action == Action::Emoji) {
    if (router::screen().acceptsText()) {
      symbols.setBackspaceCloses(board::backspaceIsBack());
      if (k.action == Action::Symbols)
        symbols.open(router::screen().paletteLabel(), SymbolTable::Set::Symbols, board::upDownOnly());
#if RC_FEAT_EMOJI
      else
        symbols.open(U"", SymbolTable::Set::Emoji, board::upDownOnly());
#endif
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
// frame as drawn; read only, it touches nothing else. A board without PSRAM
// draws in strips and keeps no frame, so it answers that it has none.
SerialCommand console;

void sendScreenshot() {
  const uint16_t* f = lvui::frame();
  const int w = displayW(), h = displayH();
  if (!f) {
    Serial.printf("\n@@NOSHOT no frame on this board (it draws in strips)\n");
    return;
  }
  if (w <= 0 || h <= 0) return;
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

#if RC_LOW_MEM
// Heap and stack headroom every 30 s, on a board without PSRAM.
void logMemory(uint32_t now) {
  static uint32_t last = 0;
  if (last && now - last < 30000) return;
  last = now;
  lv_mem_monitor_t lv;
  lv_mem_monitor(&lv);
  Serial.printf("[mem] int %u blk %u min %u | ui stack free %u | lvgl %u max %u of %u\n",
                unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
                unsigned(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)),
                unsigned(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL)),
                unsigned(uxTaskGetStackHighWaterMark(nullptr)),
                unsigned(lv.total_size - lv.free_size), unsigned(lv.max_used), unsigned(lv.total_size));
}
#endif

// The keys LVGL's views emitted this frame, as touch keys through handleKey.
void runTapKeys() {
  if (tapKeys.empty()) return;
  std::vector<TapKey> ks;
  ks.swap(tapKeys);
  if (dropTaps) return;
  if (app().cfg.tapVibrate && board::hasHaptic()) board::haptic(kTapBuzzMs);
  const bool menuBefore = Overlay::anyOpen();
  touchKey = true;
  for (const TapKey& q : ks) {
    handleKey(q.key);
    if (q.held && !menuBefore && Overlay::anyOpen()) pressMenu = true;
    if (q.key.action == Action::Focus && power == Power::On && !board::hasKeyboard() &&
        router::screen().acceptsText())
      showOsk(true);
    // The emoji tray opened from the keyboard takes its place.
    if (q.key.action == Action::Emoji && oskShown && symbols.isOpen()) {
      showOsk(false);
      oskBehindTray = true;
    }
  }
  touchKey = false;
}

// One frame (spec §5): events, input, state, then LVGL draws what changed.
void taskMain(void*) {
  ThemeId shown = app().cfg.theme;
  // The screen and power state the last frame's changes were checked against.
  router::Id beforeScreen = router::current();
  Power beforePower = power;
  for (;;) {
    uint32_t start = millis();
#if RC_LOW_MEM
    logMemory(start);
#endif
    dropTaps = false;
    // Worker and network events first (spec §5), then keys and touch.
    Event e;
    while (nextEvent(e)) handleEvent(e);
    // One touch sample a frame, for LVGL's pointer and the gestures.
    // Touch acts only while lit, and wakes a dark screen only on boards
    // where any input wakes (that finger then does nothing until it lifts).
    const bool readTouch = board::hasTouch() && (power == Power::On || board::anyInputWakes());
    touch.down = false;
    if (readTouch) {
      int16_t tx = touch.x, ty = touch.y;
      touch.down = board::readTouch(tx, ty);
      if (touch.down) {
        touch.x = tx;
        touch.y = ty;
      }
    }
    if (power != Power::On && readTouch) {
      const bool waking = board::doubleTapWakes() ? doubleTap.feed(touch.down, millis()) : touch.down;
      if (waking && nowMs() - lockedAt >= kLockQuietMs) {
        wake();
        touchSwallow = true;
        gestures.reset();
      }
    } else if (power == Power::On && readTouch) {
      if (touchSwallow && !touch.down) touchSwallow = false;
      if (touch.down) {
        lastInputMs = nowMs();
        setLit(255);
      }
      // Edge swipes are the gesture recogniser's; LVGL's views take the rest.
      Gesture g;
      if (!touchSwallow && gestures.feed(touch.down, touch.x, touch.y, millis(), g) &&
          (g.kind == GestureKind::EdgeBack || g.kind == GestureKind::EdgeOptions)) {
        dropTaps = true;
        if (app().cfg.tapVibrate && board::hasHaptic()) board::haptic(kTapBuzzMs);
        touchKey = true;
        handleKey(KeyEvent{g.kind == GestureKind::EdgeBack ? Action::Back : Action::Options});
        touchKey = false;
      }
    }
    board::textEntry(power == Power::On && router::screen().acceptsText());
    KeyEvent key;
    while (board::pollKey(key)) handleKey(key);
    applyKbLight();
    applyBacklight();
    applyStatusLights();
    if (board::webCompose()) runWebCompose();
    applyNotifyLight();
    switch (power_service::step()) {
      case PowerStep::Stay: break;
      case PowerStep::Sleep:
        finalScreen("Sleeping \u2014 updated " + clockText(), false);
        power_service::sleep();
        hideFinal();
        break;
      case PowerStep::PowerOff:
        finalScreen("Battery empty \u2014 charge to turn on", true);
        power_service::powerOff();
        hideFinal();
        break;
    }

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
      if (router::current() != beforeScreen) {
        showOsk(false);
        oskBehindTray = false;
      }
    }
    if (oskShown && !router::screen().acceptsText()) showOsk(false);
    if (oskWanted && !board::canType()) {
      oskWanted = false;
      if (power == Power::On && router::current() == beforeScreen && router::screen().acceptsText()) {
        canned.open(app().cfg.phrases, RC_FEAT_EMOJI != 0);
        if (!canned.isOpen()) ui::banner("No phrases: add them in web config", true);
      }
    }
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
    if (!symbols.isOpen()) cannedEmoji = false;
    if (canned.isOpen() && (router::current() != beforeScreen || power != Power::On ||
                            !router::screen().acceptsText()))
      canned.close();
    // Settings and web config change the theme between frames: restyle, and
    // build it all again (views copy some colours when they are made).
    if (app().cfg.theme != shown) {
      shown = app().cfg.theme;
      lvui::applyTheme(currentTheme());
      needResize = true;
    }
    // A text size, rotation or keyboard dock change: new fonts and sizes,
    // so the layers and the screen are built again (beforeBuild relays out).
    if (needResize || wantedSize() != shownSize || wantHeader() != shownHeader) router::rebuild();
    if (!Overlay::anyOpen() || router::current() != beforeScreen) pressMenu = false;
    setCursorShown(baseCursor || pressMenu);
    beforeScreen = router::current();
    beforePower = power;
    syncLayers(now);
    if (power == Power::On) router::update(now);
    // Dark: nothing to draw, and the panel sleeps.
    if (power != Power::Dark) lv_timer_handler();
    runTapKeys();
    pollConsole();
    uint32_t spent = millis() - start;
    vTaskDelay(pdMS_TO_TICKS(spent < kFrameMs ? kFrameMs - spent : 1));
  }
}

}  // namespace

void start() {
  symbols.setEmoji(RC_FEAT_EMOJI != 0);
  if (board::canRotate()) board::setRotation(app().cfg.rotation);
  if (TextSize only; board::fixedTextSize(only)) {
    app().cfg.textSize = only;
  } else {
#if RC_LOW_MEM
    if (app().cfg.textSize == TextSize::Compact) app().cfg.textSize = TextSize::Comfortable;
#endif
  }
  power_service::begin();
  baseCursor = board::hasKeys() || board::sideNav();
  setCursorShown(baseCursor);
  lvui::setTouchOnly(!board::hasKeys());
  Overlay::setConfirmButtons(board::hasTouch() && !board::hasKeys());
  // LVGL starts here, before the UI task that then owns it runs.
  lvglBegin(readTouchCached);
  lvui::applyTheme(currentTheme());
  lvui::setKeySink(queueKey);
  router::setBeforeBuild(beforeBuild);
  router::go(router::Id::Splash);
  board::backlight(true);
  sentBacklight = 255;
  applyBacklight();
  // Core 1 beside the network's core 0; a single-core chip (the X4's C3) has only 0.
#if RC_LOW_MEM
  // Measured: about 3.4 KB used. The heap left over is what TLS runs in.
  constexpr uint32_t kUiStack = 10240;
#else
  constexpr uint32_t kUiStack = 16384;
#endif
  xTaskCreatePinnedToCore(taskMain, "ui", kUiStack, nullptr, 2, nullptr, portNUM_PROCESSORS > 1 ? 1 : 0);
}

void setTextSize(TextSize s) {
  if (TextSize only; board::fixedTextSize(only)) s = only;
  if (s == TextSize::Large && !board::hasLargeText()) s = TextSize::Compact;  // the cycle wraps
  app().cfg.textSize = s;
  settings::saveUiPrefs(app().cfg);
  needResize = true;
}

void setChatStyle(ChatStyle s) {
  app().cfg.chatStyle = s;
  settings::saveChatStyle(s);
}

void setUnreadFilter(UnreadFilter f) {
  app().cfg.unreadFilter = f;
  settings::saveUnreadFilter(f);
#if RC_FEAT_ENTRIES
  lockedFeed().with([f](FeedState& s) {
    s.unreadFilter = f;
    if (f != UnreadFilter::Mentions) return;
    s.socialDropped = true;
    auto& n = s.notices;
    n.erase(std::remove_if(n.begin(), n.end(), [](const FeedNotice& x) { return x.social; }), n.end());
  });
#endif
}

namespace {
TextSize nextSize(TextSize s) {
  const TextSize n = nextTextSize(s);
  return n == TextSize::Large && !board::hasLargeText() ? nextTextSize(n) : n;
}
}  // namespace

std::u32string textSizeItem() { return U"Text size: " + textSizeLabel(nextSize(app().cfg.textSize)); }

void cycleTextSize() { setTextSize(nextSize(app().cfg.textSize)); }

void setRotation(uint8_t r) { applyRotation(r); }

bool fromTouch() { return touchKey; }

uint32_t inputSeq() { return inputCount; }

std::string monsterWord() { return words.next(esp_random(), app().mascot.phrases); }

void showKeyboard() { oskWanted = true; }

ComposeView composeView() {
  std::lock_guard<std::mutex> l(composeMu);
  return composeShown;
}

std::string webCompose(ComposeReq r) {
  std::lock_guard<std::mutex> l(composeMu);
  if (composeWaiting || composeBusy) return "Still sending the last one";
  if (r.kind == ComposeKind::Mail && !RC_FEAT_CMAIL) return "C-Mail is not on this device";
  if (r.kind == ComposeKind::Thread && !RC_FEAT_GUILDS) return "Guilds are not on this device";
  std::string err = checkCompose(r, composeShown.takesText);
  if (!err.empty()) return err;
  composeSlot = std::move(r);
  composeWaiting = true;
  return "";
}

bool typingElsewhere(const char* what) {
  if (board::canType()) return false;
  banner(std::string(what) + " from web config", true);
  return true;
}

void banner(const std::string& text, bool error) {
  topBanner.show(utf8ToU32(text), nowMs(), error ? 6000 : 4000, error);
}

void redrawAll() { needResize = true; }

void previewBrightness(int pct) {
  brightnessPreview = pct;
  applyBacklight();
}

void previewLedLevel(int level) {
  ledLevelPreview = level;
  applyStatusLights();
}

void notifyLightPreview(LedColor c, NotifySource s) {
  if (NotifyBlinker* b = blinkerFor(s)) b->preview(c, millis());
}

}  // namespace rc::ui
