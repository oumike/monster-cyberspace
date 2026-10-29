#include "app/app_state.h"
#include "app/ui_task.h"
#include "chat/chat_source.h"
#include "screens/button_pad.h"
#include "screens/draw.h"
#include "screens/screens.h"
#include "store/locked_chat.h"

namespace rc {

// Row 0 is "New conversation…"; row i + 1 is ids_[i].

void CmailListScreen::enter(const std::string&) {
  editing_ = busy_ = false;
  input_.clear();
  chatSource().command(Command{CmdType::RefreshConversations});
}

void CmailListScreen::open(int i) {
  if (i == 0) {
    editing_ = true;
    input_.clear();
  } else if (i > 0 && i - 1 < int(ids_.size())) {
    router::go(router::Id::Conversation, ids_[i - 1]);
  }
}

void CmailListScreen::onKey(const KeyEvent& k) {
  if (editing_) return editKey(k);
  if (buttons_ && k.action != Action::Left && k.action != Action::Back) return open(pad_.key(k));
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
      open(list_.selected());
      break;
    case Action::Left:
    case Action::Back:
      router::go(router::Id::Home);
      break;
    default:
      break;
  }
}

void CmailListScreen::editKey(const KeyEvent& k) {
  if (busy_) {
    if (k.action == Action::Back) busy_ = editing_ = false;
    return;
  }
  switch (k.action) {
    case Action::Char:
      if (k.ch != U' ' && k.ch != U'@') input_.insert(k.ch);
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
    case Action::Select:
      if (input_.text().empty()) break;
      if (app().conn == 0) {
        ui::banner("Offline: not sent", true);
        break;
      }
      busy_ = true;
      chatSource().command(Command{CmdType::StartConversation, input_.utf8()});
      break;
    case Action::Back:
      editing_ = false;
      break;
    default:
      break;
  }
}

void CmailListScreen::onEvent(const Event& e) {
  if (e.type != EvtType::ConversationStarted || !busy_) return;
  busy_ = false;
  if (e.ok) {
    editing_ = false;
    router::go(router::Id::Conversation, e.text);
  } else {
    ui::banner(e.text.empty() ? std::string("Couldn't start a conversation") : e.text, true);
  }
}

void CmailListScreen::render(Grid& g, const Theme& t, int64_t) {
  buttons_ = ButtonPad::active();
  std::vector<ListItem> items{ListItem{U"New conversation\u2026", U""}};
  ids_.clear();
  lockedChat().with([&](ChatState& s) {
    for (const auto& c : s.conversations) {
      std::string label = "@" + c.partner + (c.partnerDeleted ? " (deleted)" : "");
      std::string right = c.unread > 0 ? std::to_string(c.unread) : std::string();
      // Buttons show the last message on a line of its own.
      if (!buttons_ && !c.lastMessage.empty()) label += "  " + c.lastMessage;
      ListItem it{utf8ToU32(label), utf8ToU32(right), ItemTone::Normal, c.unread > 0};
      if (buttons_) it.detail = utf8ToU32(c.lastMessage);
      items.push_back(std::move(it));
      ids_.push_back(c.id);
    }
  });
  g.fill(t.bg);
  drawStatus(g, t, U"c-mail", int(ids_.size()));
  int bottom = g.rows() - 1;
  if (editing_) --bottom;
  if (buttons_) {
    pad_.render(g, t, items, 2, bottom, true, 1);
    if (ids_.empty()) putLine(g, 1, 7, "no conversations yet", t.dim, t.bg);
  } else {
    list_.setItems(std::move(items));
    const int nav = sideNavWidth(g);
    list_.render(g, 1, bottom, 0, g.cols() - nav, t);
    if (nav > 0) renderSideNav(g, 1, bottom, nav, t);
    if (ids_.empty()) putLine(g, 1, 3, "no conversations yet", t.dim, t.bg);
  }
  if (editing_) {
    input_.setDisabled(busy_);
    input_.render(g, bottom, t, U"to> @");
    drawHint(g, t, busy_ ? U"starting\u2026  back: cancel" : U"type a username  \u25CF start  back: cancel",
             busy_ ? U"starting\u2026" : U"type a username  enter: start");
  } else if (buttons_) {
    drawHint(g, t, U"", U"tap to open  drag to scroll");
  } else {
    drawHint(g, t, U"\u2191\u2193 pick  \u25CF open  \u2190 home", U"tap to open  swipe to scroll");
  }
}

}  // namespace rc
