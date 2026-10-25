#include "screens/room.h"

#include "app/app_state.h"
#include "chat/chat_source.h"
#include "screens/draw.h"
#include "store/locked_chat.h"

namespace rc {

namespace {
constexpr int64_t kStaleMs = 5 * 60 * 1000;
}

void RoomScreen::enter(const std::string& slug) {
  slug_ = slug.empty() ? std::string("general") : slug;
  log_.follow();
  version_ = UINT32_MAX;
  app().unreadMentions = 0;
  chatSource().command(Command{CmdType::OpenRoom, slug_});
}

void RoomScreen::leave() { chatSource().command(Command{CmdType::CloseRoom}); }

void RoomScreen::onKey(const KeyEvent& k) {
  switch (k.action) {
    case Action::Up:
      if (log_.atTop()) chatSource().command(Command{CmdType::LoadOlder});
      log_.up();
      break;
    case Action::Down:
      log_.down();
      break;
    case Action::Back:
    case Action::Left:
      if (!log_.following()) log_.follow();
      else router::go(router::Id::RoomList);
      break;
    default:
      break;
  }
}

void RoomScreen::render(Grid& g, const Theme& t, int64_t nowMs) {
  int online = 0;
  lockedChat().with([&](ChatState& s) {
    online = int(visiblePresence(s.presence, nowMs, kStaleMs).size());
    if (s.store.version() == version_ && g.cols() == cols_) return;
    version_ = s.store.version();
    cols_ = g.cols();
    LayoutOptions o;
    o.size = app().cfg.textSize;
    o.cols = g.cols();
    o.selfUsername = s.self;
    o.revealed = &s.revealed;
    o.theme = &t;
    log_.setLines(layoutRoom(s.store.messages(), o));
  });
  g.fill(t.bg);
  drawStatus(g, t, utf8ToU32("#" + slug_), online);
  log_.render(g, 1, g.rows() - 1, 0, t);
  drawHint(g, t, log_.following() ? U"\u2191 scroll  back: rooms" : U"\u2191\u2193 scroll  back: newest");
}

}  // namespace rc
