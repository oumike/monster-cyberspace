#include "screens/room.h"

#include <algorithm>
#include <cctype>

#include "app/app_state.h"
#include "app/ui_task.h"
#include "chat/chat_source.h"
#include "screens/draw.h"
#include "store/locked_chat.h"
#include "ui/widgets/palette.h"
#include "ui/wrap.h"
#include "util/base64.h"

namespace rc {

namespace {

constexpr int64_t kMarkReadMs = 10000;
constexpr int kLongMessageLines = 6;

std::string lower(std::string s) {
  for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

bool hasStyle(const CircMessage& m, const char* s) {
  return std::find(m.styles.begin(), m.styles.end(), s) != m.styles.end();
}

// The first link a message carries: an attachment, else a URL in the text.
std::string firstUrl(const CircMessage& m) {
  if (!m.gifUrl.empty()) return m.gifUrl;
  if (!m.imageUrl.empty()) return m.imageUrl;
  if (m.hasAudio && !m.audio.src.empty()) return m.audio.src;
  size_t p = m.content.find("https://");
  if (p == std::string::npos) p = m.content.find("http://");
  if (p == std::string::npos) return "";
  size_t e = m.content.find_first_of(" \n\t", p);
  return m.content.substr(p, e == std::string::npos ? std::string::npos : e - p);
}

bool compact() { return app().cfg.textSize == TextSize::Compact; }

}  // namespace

void RoomScreen::enter(const std::string& slug) {
  slug_ = slug.empty() && !dm_ ? std::string("general") : slug;
  log_.follow();
  input_.clear();
  overlay_ = Overlay::None;
  reasonMode_ = false;
  pending_.clear();
  olderAsked_ = false;
  version_ = UINT32_MAX;
  relayout_ = true;
  lastMarkRead_ = 0;
  typingOn_ = false;
  lastTypingPost_ = 0;
  if (dm_) {
    partner_ = lockedChat().with([&](ChatState& s) {
      for (const auto& c : s.conversations)
        if (c.id == slug_) return c.partner;
      return std::string("?");
    });
    chatSource().command(Command{CmdType::OpenConversation, slug_});
    return;
  }
  app().unreadMentions = 0;
  chatSource().command(Command{CmdType::OpenRoom, slug_});
  if (app().cfg.drawerOpen) chatSource().command(Command{CmdType::OpenPresence});
}

void RoomScreen::leave() {
  pending_.clear();
  // Leaving with text typed: clear the flag rather than let it age out.
  stopTyping();
  chatSource().command(Command{CmdType::CloseRoom});
}

bool RoomScreen::drawerOpen() const { return !dm_ && app().cfg.drawerOpen; }

void RoomScreen::noteTyping() {
  if (!dm_) return;
  if (input_.text().empty()) {
    stopTyping();
    return;
  }
  if (app().conn == 0) return;
  int64_t now = nowEpochMs();
  int64_t beat = lockedChat().with([](ChatState& s) { return s.typingCadence.heartbeatMs; });
  if (typingOn_ && now - lastTypingPost_ < beat) return;
  typingOn_ = true;
  lastTypingPost_ = now;
  chatSource().command(Command{CmdType::Typing, slug_, "", 1});
}

void RoomScreen::stopTyping() {
  if (!dm_ || !typingOn_) return;
  typingOn_ = false;
  lastTypingPost_ = 0;
  if (app().conn != 0) chatSource().command(Command{CmdType::Typing, slug_, "", 0});
}

bool RoomScreen::hasOverlay() const {
  // The drawer is not an overlay: it persists across rooms.
  return overlay_ != Overlay::None || reasonMode_ || !log_.following();
}

void RoomScreen::closeOverlays() {
  overlay_ = Overlay::None;
  if (reasonMode_) {
    reasonMode_ = false;
    input_.setText(savedInput_);
  }
  log_.follow();
}

void RoomScreen::setDrawer(bool open) {
  if (app().cfg.drawerOpen == open) return;
  app().cfg.drawerOpen = open;
  settings::saveUiPrefs(app().cfg);
  chatSource().command(Command{open ? CmdType::OpenPresence : CmdType::ClosePresence});
}

bool RoomScreen::isMuted(const std::string& name) { return muted_.count(lower(name)) > 0; }

void RoomScreen::insertText(const std::u32string& s) {
  for (char32_t c : s) input_.insert(c);
}

// ---- keys ----

void RoomScreen::onKey(const KeyEvent& k) {
  if (overlay_ == Overlay::Menu) return menuKey(k);
  if (overlay_ != Overlay::None) return viewerKey(k);
  if (reasonMode_) return reasonKey(k);
  typingKey(k);
}

void RoomScreen::typingKey(const KeyEvent& k) {
  const bool drawer = drawerOpen();
  switch (k.action) {
    case Action::Char:
      input_.insert(k.ch);
      noteTyping();
      break;
    case Action::Backspace:
      input_.backspace();
      noteTyping();
      break;
    case Action::ClearLine:
      input_.clear();
      noteTyping();
      break;
    case Action::Palette: {
      std::vector<ListItem> items;
      menuKeys_.clear();
      for (const auto& e : paletteEntries(dm_ ? PaletteKind::Cmail : PaletteKind::Circ)) {
        std::u32string label = e.label;
        if (e.isTextSize) label = compact() ? U"Text size: Comfortable" : U"Text size: Compact";
        items.push_back(ListItem{label, U"", defaultTheme().fg});
        menuKeys_.push_back(e.isTextSize ? std::string("size") : u32ToUtf8(e.insert));
      }
      openMenu(Menu::Palette, U"commands", std::move(items));
      break;
    }
    case Action::Up:
      if (drawer) {
        drawer_.up();
      } else {
        if (log_.atTop() && !olderAsked_) {
          bool more = lockedChat().with([](ChatState& s) { return !s.noOlder; });
          if (more) chatSource().command(Command{CmdType::LoadOlder});
          olderAsked_ = true;
        }
        log_.up();
      }
      break;
    case Action::Down:
      if (drawer) drawer_.down();
      else log_.down();
      break;
    case Action::Left:
      if (drawer) setDrawer(false);
      else input_.left();
      break;
    case Action::Right:
      if (!input_.right() && !drawer && !dm_) setDrawer(true);
      break;
    case Action::Select:
      if (drawer) {
        std::string who = drawer_.selectedUsername();
        if (!who.empty()) openPersonMenu(who);
      } else if (!log_.following()) {
        openMessageMenu();
      } else if (!input_.text().empty()) {
        send();
      } else {
        openRoomMenu();
      }
      break;
    case Action::Back:
      if (drawer) setDrawer(false);
      else if (!log_.following()) log_.follow();
      else router::go(dm_ ? router::Id::CmailList : router::Id::RoomList);
      break;
    default:
      break;
  }
}

void RoomScreen::send() {
  if (app().conn == 0) {
    ui::banner("Offline: not sent", true);
    return;
  }
  if (!pending_.empty()) {
    ui::banner("Still sending\u2026", true);
    return;
  }
  pending_ = input_.text();
  input_.clear();
  typingOn_ = false;  // the server clears the flag on send
  relayout_ = true;
  chatSource().command(Command{CmdType::Send, slug_, u32ToUtf8(pending_)});
}

void RoomScreen::reasonKey(const KeyEvent& k) {
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
    case Action::Select:
      chatSource().command(Command{CmdType::Flag, target_, input_.utf8()});
      reasonMode_ = false;
      input_.setText(savedInput_);
      log_.follow();
      break;
    case Action::Back:
      reasonMode_ = false;
      input_.setText(savedInput_);
      break;
    default:
      break;
  }
}

void RoomScreen::viewerKey(const KeyEvent& k) {
  switch (k.action) {
    case Action::Up:
      viewTop_ = std::max(0, viewTop_ - 1);
      break;
    case Action::Down:
      ++viewTop_;  // clamped in render
      break;
    case Action::Left:
      if (overlay_ == Overlay::Reader && viewLeft_ > 0) viewLeft_ = std::max(0, viewLeft_ - 8);
      else overlay_ = Overlay::None;
      break;
    case Action::Right:
      viewLeft_ += 8;  // clamped in render
      break;
    case Action::Back:
    case Action::Select:
      overlay_ = Overlay::None;
      break;
    default:
      break;
  }
}

// ---- menus ----

void RoomScreen::openMenu(Menu m, std::u32string title, std::vector<ListItem> items) {
  menu_ = m;
  menuTitle_ = std::move(title);
  menuList_.setItems(std::move(items));
  menuList_.setSelected(0);
  overlay_ = Overlay::Menu;
}

void RoomScreen::openRoomMenu() {
  const Theme& t = defaultTheme();
  if (dm_) {
    menuKeys_ = {"read", "size", "close"};
    openMenu(Menu::Room, utf8ToU32("@" + partner_),
             {ListItem{U"Mark read", U"", t.fg},
              ListItem{compact() ? U"Text size: Comfortable" : U"Text size: Compact", U"", t.fg},
              ListItem{U"Close conversation", U"", t.fg}});
    return;
  }
  menuKeys_ = {"who", "mute", "unmute", "read", "size", "leave"};
  openMenu(Menu::Room, utf8ToU32("#" + slug_),
           {ListItem{U"Who's here", U"", t.fg}, ListItem{U"Mute\u2026", U"", t.fg},
            ListItem{U"Unmute\u2026", U"", t.fg}, ListItem{U"Mark read", U"", t.fg},
            ListItem{compact() ? U"Text size: Comfortable" : U"Text size: Compact", U"", t.fg},
            ListItem{U"Leave room", U"", t.fg}});
}

void RoomScreen::openPersonMenu(const std::string& name) {
  const Theme& t = defaultTheme();
  target_ = name;
  bool muted = isMuted(name);
  menuKeys_ = {"mention", muted ? "unmute1" : "mute1"};
  openMenu(Menu::Person, utf8ToU32(name),
           {ListItem{U"Mention", U"", t.fg}, ListItem{muted ? U"Unmute" : U"Mute", U"", t.fg}});
}

void RoomScreen::openMessageMenu() {
  const Theme& t = defaultTheme();
  std::string id = log_.selectedId();
  CircMessage m;
  bool found = false, revealed = false;
  lockedChat().with([&](ChatState& s) {
    if (CircMessage* p = s.store.find(id)) {
      m = *p;
      found = true;
      revealed = s.revealed.count(id) > 0;
    }
  });
  if (!found) return;
  target_ = id;
  std::vector<ListItem> items;
  menuKeys_.clear();
  auto add = [&](const char32_t* label, const char* key, uint16_t fg) {
    items.push_back(ListItem{label, U"", fg});
    menuKeys_.push_back(key);
  };
  bool own = lower(m.username) == lower(self_);
  if (!m.deleted) {
    if (!own && !dm_) add(U"Reply", "reply", t.fg);
    if (hasStyle(m, "spoiler") && !revealed) add(U"Reveal", "reveal", t.fg);
    else if (hasStyle(m, "art") || m.content.size() > 200) add(U"Show full", "full", t.fg);
    if (!firstUrl(m).empty()) add(U"Show link", "link", t.fg);
    // C-Mail has no delete or report.
    if (dm_) {
    } else if (own) {
      add(U"Delete", "delete", t.error);
    } else {
      add(U"Report", "report", t.error);
    }
  }
  if (items.empty()) return;
  openMenu(Menu::Message, utf8ToU32(m.username.empty() ? std::string("message") : m.username),
           std::move(items));
}

void RoomScreen::menuKey(const KeyEvent& k) {
  switch (k.action) {
    case Action::Up:
      menuList_.up();
      break;
    case Action::Down:
      menuList_.down();
      break;
    case Action::Back:
    case Action::Left:
      overlay_ = Overlay::None;
      break;
    case Action::Select:
    case Action::Right:
      pickMenu();
      break;
    default:
      break;
  }
}

void RoomScreen::pickMenu() {
  int i = menuList_.selected();
  if (i < 0 || i >= int(menuKeys_.size())) return;
  const std::string key = menuKeys_[i];
  const Theme& t = defaultTheme();
  overlay_ = Overlay::None;
  switch (menu_) {
    case Menu::Palette:
      if (key == "size") ui::setTextSize(compact() ? TextSize::Comfortable : TextSize::Compact);
      else insertText(utf8ToU32(key));
      break;
    case Menu::Room:
      if (key == "who") {
        log_.follow();
        setDrawer(true);
      } else if (key == "mute") {
        std::vector<ListItem> items;
        menuKeys_.clear();
        lockedChat().with([&](ChatState& s) {
          for (const auto& p : visiblePresence(s.presence, nowEpochMs(), s.cadence.staleAfterMs)) {
            if (lower(p.username) == lower(self_) || isMuted(p.username)) continue;
            items.push_back(ListItem{utf8ToU32(p.username), U"", t.fg});
            menuKeys_.push_back(p.username);
          }
        });
        if (items.empty()) ui::banner("Nobody here to mute", false);
        else openMenu(Menu::MutePick, U"mute who?", std::move(items));
      } else if (key == "unmute") {
        std::vector<ListItem> items;
        menuKeys_.clear();
        for (const auto& n : muted_) {
          items.push_back(ListItem{utf8ToU32(n), U"", t.fg});
          menuKeys_.push_back(n);
        }
        if (items.empty()) ui::banner("Nobody muted in this room", false);
        else openMenu(Menu::UnmutePick, U"unmute who?", std::move(items));
      } else if (key == "read") {
        chatSource().command(Command{CmdType::MarkRead, slug_});
        lastMarkRead_ = nowEpochMs();
      } else if (key == "size") {
        ui::setTextSize(compact() ? TextSize::Comfortable : TextSize::Compact);
      } else if (key == "leave") {
        router::go(router::Id::RoomList);
      } else if (key == "close") {
        router::go(router::Id::CmailList);
      }
      break;
    case Menu::MutePick:
    case Menu::UnmutePick:
      if (app().conn == 0) {
        ui::banner("Offline: not sent", true);
        break;
      }
      chatSource().command(
          Command{CmdType::Send, slug_, (menu_ == Menu::MutePick ? "/mute " : "/unmute ") + key});
      break;
    case Menu::Person:
      if (key == "mention") {
        insertText(utf8ToU32("@" + target_ + " "));
        setDrawer(false);
      } else if (app().conn == 0) {
        ui::banner("Offline: not sent", true);
      } else {
        chatSource().command(Command{CmdType::Send, slug_,
                                     (key == "mute1" ? "/mute " : "/unmute ") + target_});
      }
      break;
    case Menu::Message: {
      CircMessage m;
      bool found = lockedChat().with([&](ChatState& s) {
        CircMessage* p = s.store.find(target_);
        if (p) m = *p;
        return p != nullptr;
      });
      if (!found) break;
      if (key == "reply") {
        insertText(utf8ToU32("@" + m.username + " "));
        log_.follow();
      } else if (key == "reveal") {
        lockedChat().with([&](ChatState& s) { s.revealed.insert(target_); });
        relayout_ = true;
      } else if (key == "full") {
        openReader(m);
      } else if (key == "link") {
        openLink(firstUrl(m));
      } else if (key == "delete") {
        menuKeys_ = {"cancel", "delete"};
        openMenu(Menu::ConfirmDelete, U"delete this message?",
                 {ListItem{U"Cancel", U"", t.fg}, ListItem{U"Delete", U"", t.error}});
      } else if (key == "report") {
        menuKeys_ = {"cancel", "report"};
        openMenu(Menu::ConfirmReport, U"report this message?",
                 {ListItem{U"Cancel", U"", t.fg}, ListItem{U"Report\u2026", U"", t.error}});
      }
      break;
    }
    case Menu::ConfirmDelete:
      if (key == "delete") {
        chatSource().command(Command{CmdType::Delete, target_});
        log_.follow();
      }
      break;
    case Menu::ConfirmReport:
      if (key == "report") {
        savedInput_ = input_.text();
        input_.clear();
        reasonMode_ = true;
      }
      break;
  }
}

// ---- viewers ----

void RoomScreen::openReader(const CircMessage& m) {
  viewLines_.clear();
  std::string art;
  if (hasStyle(m, "art") && base64Decode(m.content, art)) {
    std::u32string a = utf8ToU32(art);
    size_t start = 0;
    while (start <= a.size()) {
      size_t nl = a.find(U'\n', start);
      if (nl == std::u32string::npos) nl = a.size();
      std::u32string line = a.substr(start, nl - start);
      if (!line.empty() && line.back() == U'\r') line.pop_back();
      viewLines_.push_back(line);
      start = nl + 1;
    }
  } else {
    viewLines_.push_back(utf8ToU32(m.content));
  }
  wrapView_ = art.empty();
  viewTop_ = viewLeft_ = 0;
  overlay_ = Overlay::Reader;
}

void RoomScreen::openLink(const std::string& url) {
  link_.open(url);
  overlay_ = Overlay::Link;
}

// ---- events ----

void RoomScreen::onEvent(const Event& e) {
  switch (e.type) {
    case EvtType::SendDone:
      if (!e.ok) {
        // Put the text back so nothing typed is lost.
        if (!pending_.empty()) {
          std::u32string rest = input_.text();
          input_.setText(pending_ + rest);
        }
        ui::banner(e.text.empty() ? std::string("Not sent") : e.text, true);
      }
      pending_.clear();
      relayout_ = true;
      break;
    case EvtType::OlderLoaded:
      olderAsked_ = false;
      relayout_ = true;
      break;
    case EvtType::RoomChanged:
      relayout_ = true;
      break;
    default:
      break;
  }
}

// ---- drawing ----

void RoomScreen::render(Grid& g, const Theme& t, int64_t) {
  const bool small = compact();
  const int inputRow = small ? g.rows() - 2 : g.rows() - 1;
  int logBottom = inputRow;
  const int64_t now = nowEpochMs();
  const bool drawer = drawerOpen();
  int online = 0;
  bool markRead = false;
  std::vector<std::string> typers;

  // Never send a command while holding the chat lock: a source may take it.
  lockedChat().with([&](ChatState& s) {
    auto people = visiblePresence(s.presence, now, s.cadence.staleAfterMs);
    online = int(people.size());
    if (drawer) drawer_.setPeople(std::move(people), now, s.cadence.idleAfterMs);
    self_ = s.self;
    // A flag going stale produces no event, so this is re-checked each frame.
    if (dm_) typers = typingNames(s.typing, s.self, now, s.typingCadence.staleAfterMs);
    std::string roomId;
    for (const auto& r : s.rooms)
      if (r.slug == slug_) roomId = r.id;
    std::set<std::string> muted;
    for (const std::string& key : {slug_, roomId}) {
      auto it = s.muted.find(key);
      if (!key.empty() && it != s.muted.end()) muted.insert(it->second.begin(), it->second.end());
    }
    if (muted != muted_) {
      muted_ = std::move(muted);
      relayout_ = true;
    }
    // Mark read while the newest messages are on screen.
    if (log_.following() && overlay_ == Overlay::None && !s.store.messages().empty()) {
      int64_t newest = s.store.newestTimestamp();
      auto it = s.readAt.find(slug_);
      bool behind = it == s.readAt.end() || newest > it->second;
      if (behind && now - lastMarkRead_ >= kMarkReadMs) {
        lastMarkRead_ = now;
        s.readAt[slug_] = newest;
        markRead = true;
      }
    }
    if (!relayout_ && s.store.version() == version_ && g.cols() == cols_) return;
    relayout_ = false;
    version_ = s.store.version();
    cols_ = g.cols();
    LayoutOptions o;
    o.size = app().cfg.textSize;
    o.cols = g.cols();
    o.selfUsername = s.self;
    o.muted = &muted_;
    o.revealed = &s.revealed;
    o.theme = &t;
    std::vector<DisplayLine> lines = layoutRoom(s.store.messages(), o);
    if (!pending_.empty()) {
      for (const auto& w : wrapText(U"sending\u2026 " + pending_, g.cols(), g.cols())) {
        DisplayLine l;
        l.spans.push_back({w, t.dim});
        lines.push_back(std::move(l));
      }
    }
    log_.setLines(std::move(lines));

  });

  if (markRead) chatSource().command(Command{CmdType::MarkRead, slug_});

  g.fill(t.bg);
  drawStatus(g, t, utf8ToU32(dm_ ? "@" + partner_ : "#" + slug_), dm_ ? -1 : online);

  if (overlay_ == Overlay::Link) {
    link_.render(g, t);
    return;
  }
  if (overlay_ == Overlay::Reader) {
    // A full-screen viewer below the status bar.
    std::vector<std::u32string> rows =
        wrapView_ ? wrapText(viewLines_[0], g.cols() - 2, g.cols() - 2) : viewLines_;
    const int bottom = g.rows() - 1;
    int avail = bottom - 1;
    int maxTop = std::max(0, int(rows.size()) - avail);
    viewTop_ = std::min(viewTop_, maxTop);
    int widest = 0;
    for (const auto& l : rows) widest = std::max(widest, int(l.size()));
    viewLeft_ = std::min(viewLeft_, std::max(0, widest - g.cols()));
    for (int i = 0; i < avail && viewTop_ + i < int(rows.size()); ++i) {
      const auto& line = rows[viewTop_ + i];
      if (int(line.size()) > viewLeft_)
        g.putText(0, 1 + i, std::u32string_view(line).substr(viewLeft_), t.fg, t.bg, g.cols());
    }
    drawHint(g, t, U"\u2191\u2193\u2190\u2192 scroll  back: close");
    return;
  }

  int reserve = drawer ? Drawer::width(app().cfg.textSize) : 0;
  if (!typers.empty()) {
    // One row above the input for "name is typing…".
    --logBottom;
    std::string who = typers[0];
    for (size_t i = 1; i < typers.size(); ++i) who += ", " + typers[i];
    putLine(g, 1, logBottom, who + " is typing\u2026", t.dim, t.bg);
  }
  log_.render(g, 1, logBottom, reserve, t);
  if (drawer) drawer_.render(g, 1, logBottom, t);

  if (overlay_ == Overlay::Menu) {
    int n = int(menuList_.size());
    int h = std::min(n, logBottom - 2);
    int top = logBottom - h - 1;
    int width = std::min(g.cols() - 2, 30);
    int left = g.cols() - width - 1 - reserve;
    if (left < 1) left = 1;
    for (int c = 0; c < width; ++c) g.put(left + c, top, U' ', t.barFg, t.barBg);
    g.putText(left + 1, top, menuTitle_, t.barFg, t.barBg, width - 2);
    menuList_.render(g, top + 1, logBottom, left, width, t);
  }

  bool offline = app().conn == 0;
  input_.setDisabled(offline || overlay_ != Overlay::None);
  input_.render(g, inputRow, t, reasonMode_ ? U"reason> " : U"> ");

  if (small) {
    const char32_t* hint;
    if (reasonMode_) hint = U"type a reason (optional)  \u25CF report  back: cancel";
    else if (overlay_ == Overlay::Menu) hint = U"\u2191\u2193 pick  \u25CF choose  back: close";
    else if (drawer) hint = U"\u2191\u2193 pick  \u25CF menu  \u2190 close";
    else if (!log_.following()) hint = U"\u25CF actions  \u2193 back to typing";
    else if (offline) hint = U"offline: reading only";
    else if (dm_) hint = U"\u25CF send  \u2191\u2193 scroll  ctrl cmds";
    else hint = U"\u25CF send  \u2191\u2193 scroll  \u2192 who's here  ctrl cmds";
    drawHint(g, t, hint);
  }
}

}  // namespace rc
