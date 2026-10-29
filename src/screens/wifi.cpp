#include <algorithm>

#include "app/app_state.h"
#include "app/ui_task.h"
#include "chat/chat_source.h"
#include "screens/draw.h"
#include "screens/screens.h"

namespace rc {

namespace {
// The saved network menu.
enum MenuRow { kConnect, kNewPassword, kForget };

std::u32string barsText(int bars) {
  if (bars < 0) return U"away";
  return std::u32string(U"\u2582\u2584\u2586\u2588").substr(0, size_t(bars)) +
         std::u32string(size_t(4 - bars), U' ');
}

bool isSaved(const std::string& ssid) {
  const auto& w = app().cfg.wifi;
  return std::any_of(w.begin(), w.end(), [&](const WifiNet& n) { return n.ssid == ssid; });
}
}  // namespace

void WifiScreen::enter(const std::string& arg) {
  fromSetup_ = arg == "setup";
  mode_ = Mode::List;
  menu_.close();
  input_.clear();
  note_.clear();
  list_.setSelected(0);
  pad_.reset();
  scan();
}

void WifiScreen::leave() {
  input_.clear();
}

void WifiScreen::closeOverlays() {
  menu_.close();
  if (mode_ == Mode::Name || mode_ == Mode::Password) mode_ = Mode::List;
  input_.clear();
}

void WifiScreen::scan() {
  scanning_ = true;
  chatSource().command(Command{CmdType::WifiScan});
}

void WifiScreen::setNote(const std::string& text, bool error) {
  note_ = text;
  noteError_ = error;
}

void WifiScreen::startEntry(Mode m, const std::string& ssid) {
  mode_ = m;
  ssid_ = ssid;
  input_.clear();
  note_.clear();
}

void WifiScreen::join(const std::string& ssid, const std::string* password) {
  Command c{CmdType::WifiJoin, ssid};
  if (password) {
    c.b = *password;
    c.n = 1;
  }
  added_ = password && !isSaved(ssid);
  ssid_ = ssid;
  mode_ = Mode::Joining;
  note_.clear();
  chatSource().command(c);
  std::fill(c.b.begin(), c.b.end(), '\0');
}

void WifiScreen::pick(int index) {
  const int n = int(rows_.size());
  if (index < 0) return;
  if (index == n) {  // Other network…
    if (app().cfg.wifi.size() >= kMaxWifiNets)
      return setNote("Five networks saved already: forget one first", true);
    return startEntry(Mode::Name, "");
  }
  if (index == n + 1) return scan();
  if (index > n) return;
  const WifiRow& r = rows_[size_t(index)];
  if (r.saved) {
    std::vector<ListItem> items{ListItem{r.connected ? U"Reconnect" : U"Connect", U""},
                                ListItem{U"New password", U""},
                                ListItem{U"Forget", U"", ItemTone::Error}};
    ssid_ = r.ssid;
    menu_.open(utf8ToU32(r.ssid), std::move(items), 0);
    return;
  }
  if (app().cfg.wifi.size() >= kMaxWifiNets)
    return setNote("Five networks saved already: forget one first", true);
  if (!r.secure) {
    const std::string none;
    return join(r.ssid, &none);
  }
  startEntry(Mode::Password, r.ssid);
}

void WifiScreen::menuKey(const KeyEvent& k) {
  if (menu_.key(k) != Overlay::Result::Picked) return;
  switch (menu_.selected()) {
    case kConnect:
      join(ssid_, nullptr);
      break;
    case kNewPassword:
      startEntry(Mode::Password, ssid_);
      break;
    case kForget: {
      Command c{CmdType::WifiForget, ssid_};
      chatSource().command(c);
      break;
    }
  }
}

void WifiScreen::entryKey(const KeyEvent& k) {
  switch (k.action) {
    case Action::Char:
      input_.insert(k.ch);
      break;
    case Action::Backspace:
      input_.backspace();
      break;
    case Action::ClearLine:
      input_.clear();
      break;
    case Action::Left:
      input_.left();
      break;
    case Action::Right:
      input_.right();
      break;
    case Action::Focus:
      input_.setCursor(k.index);
      break;
    case Action::Back:
      mode_ = Mode::List;
      input_.clear();
      break;
    case Action::Select: {
      std::string text = input_.utf8();
      if (mode_ == Mode::Name) {
        if (!validWifiSsid(text)) return setNote("Network name: 1-32 characters", true);
        if (isSaved(text)) {
          // Already saved: this sets a new password for it.
        } else if (app().cfg.wifi.size() >= kMaxWifiNets) {
          return setNote("Five networks saved already: forget one first", true);
        }
        startEntry(Mode::Password, text);
        break;
      }
      if (!validWifiPassword(text)) {
        std::fill(text.begin(), text.end(), '\0');
        return setNote("Password: 8-63 characters, or empty for an open network", true);
      }
      input_.clear();
      join(ssid_, &text);
      std::fill(text.begin(), text.end(), '\0');
      break;
    }
    default:
      break;
  }
}

void WifiScreen::onKey(const KeyEvent& k) {
  if (menu_.isOpen()) return menuKey(k);
  if (mode_ == Mode::Name || mode_ == Mode::Password) return entryKey(k);
  if (k.action == Action::Back || k.action == Action::Left) {
    router::go(fromSetup_ ? router::Id::Setup : router::Id::Settings);
    return;
  }
  if (mode_ == Mode::Joining) return;  // Back above leaves; the join goes on
  if (buttons_) return pick(pad_.key(k));
  switch (k.action) {
    case Action::Up:
      list_.up();
      break;
    case Action::Down:
      list_.down();
      break;
    case Action::Activate:
      if (k.target == uint8_t(ListId::Main) && list_.select(k.index)) onKey(KeyEvent{k.then});
      break;
    case Action::Select:
    case Action::Right:
      pick(list_.selected());
      break;
    default:
      break;
  }
}

void WifiScreen::onEvent(const Event& e) {
  if (e.type == EvtType::WifiScanned) {
    scanning_ = false;
    if (!decodeWifiScan(e.text, scan_)) scan_.clear();
    return;
  }
  if (e.type != EvtType::WifiState) return;
  switch (e.code) {
    case 1:
      if (mode_ == Mode::Joining) mode_ = Mode::List;
      setNote("Connected to " + e.text, false);
      if (fromSetup_) router::go(router::Id::Setup);
      break;
    case 2:
      setNote("Forgot " + e.text, false);
      break;
    case 0:
      break;
    default:
      if (mode_ == Mode::Joining) mode_ = Mode::List;
      setNote(e.text, true);
      // A new network that failed is not kept: the next try asks again.
      if (e.code == -1 && added_) chatSource().command(Command{CmdType::WifiForget, ssid_});
      added_ = false;
      break;
  }
}

void WifiScreen::render(Grid& g, const Theme& t, int64_t) {
  g.fill(t.bg);
  drawStatus(g, t, U"wi-fi", -1);
  const int bottom = g.rows() - 1;
  buttons_ = ButtonPad::active();

  if (mode_ == Mode::Name || mode_ == Mode::Password) {
    const bool name = mode_ == Mode::Name;
    int r = 2;
    if (!name) r = putWrapped(g, 1, r, "Password for " + ssid_, t.dim, t.bg);
    else putLine(g, 1, r++, "Network name", t.dim, t.bg);
    input_.setDisabled(false);
    input_.render(g, ++r, t);
    r += 2;
    if (!note_.empty()) r = putWrapped(g, 1, r, note_, noteError_ ? t.error : t.fg, t.bg) + 1;
    if (!name) putWrapped(g, 1, r, "Leave it empty for an open network.", t.dim, t.bg);
    drawHint(g, t, name ? U"\u25CF next  back: cancel" : U"\u25CF join  back: cancel",
             name ? U"enter: next" : U"enter: join");
    return;
  }

  if (mode_ == Mode::Joining) {
    int r = putWrapped(g, 1, 2, "Joining " + ssid_ + "\u2026", t.fg, t.bg);
    putWrapped(g, 1, r + 1, "This takes up to 12 seconds.", t.dim, t.bg);
    drawHint(g, t, U"back: leave (it keeps trying)", U"back: leave (it keeps trying)");
    return;
  }

  rows_ = wifiRows(scan_, app().cfg.wifi, wifiUp() ? wifiSsid() : std::string());
  std::vector<ListItem> items;
  for (const WifiRow& r : rows_) {
    std::u32string right = r.connected ? U"connected " : r.saved ? U"saved " : r.secure ? U"" : U"open ";
    items.push_back(ListItem{utf8ToU32(r.ssid), right + barsText(r.bars), ItemTone::Normal, r.connected});
  }
  items.push_back(ListItem{U"Other network\u2026", U""});
  items.push_back(ListItem{scanning_ ? U"Scanning\u2026" : U"Scan again", U""});

  int top = 1;
  if (!note_.empty()) top = putWrapped(g, 1, 1, note_, noteError_ ? t.error : t.fg, t.bg);
  if (buttons_) {
    pad_.render(g, t, items, top + 1, bottom, true);
  } else {
    list_.setItems(std::move(items));
    list_.render(g, top, bottom, 0, g.cols(), t);
  }
  if (menu_.isOpen()) {
    menu_.render(g, 1, bottom, 0, OverlayPlace::Centre, t);
    drawHint(g, t, U"\u2191\u2193 pick  \u25CF select  back: cancel", U"tap: select  outside: cancel");
    return;
  }
  if (buttons_) return drawHint(g, t, U"", U"tap to select  drag to scroll");
  drawHint(g, t, U"\u2191\u2193 pick  \u25CF select  back: leave", U"tap to select  swipe to scroll");
}

}  // namespace rc
