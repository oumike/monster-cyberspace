#include "feature_flags.h"
#if RC_FEAT_CMAIL

#include "app/app_state.h"
#include "app/ui_task.h"
#include "chat/chat_source.h"
#include "screens/button_pad.h"
#include "screens/lv_draw.h"
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
    if (ui::typingElsewhere("Start a C-Mail")) return;
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

std::vector<ListItem> CmailListScreen::rows() {
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
  return items;
}

void CmailListScreen::build(lv_obj_t* root) {
  v_.make(root);
  v_->listView_.create(root);
  v_->padView_.create(root);
  v_->navView_.create(root);
  // Over the buttons.
  v_->text_.create(root);
  v_->inputView_.create(root);
  v_->hint_.create(root);
  v_->status_.create(root);
  sync();
}

void CmailListScreen::update(int64_t nowMs) {
  // The conversations change with ConversationsUpdated, which
  // ui::inputSeq() counts.
  if (v_->gate_.due(v_->text_.obj(), nowMs, {ButtonPad::active()})) sync();
}

void CmailListScreen::sync() {
  const Theme& t = lvui::currentTheme();
  buttons_ = ButtonPad::active();
  std::vector<ListItem> items = rows();
  syncStatus(v_->status_, U"c-mail", int(ids_.size()));
  int bottom = v_->text_.rows() - 1;
  if (editing_) --bottom;
  lvui::setHidden(v_->listView_.obj(), buttons_);
  lvui::setHidden(v_->padView_.obj(), !buttons_);
  v_->text_.begin();
  if (buttons_) {
    pad_.sync(v_->padView_, items, 2, bottom, true, 1);
    v_->navView_.sync(0, 1, bottom, v_->text_.cols());
    if (ids_.empty()) v_->text_.line(1, 7, "no conversations yet", t.dim);
  } else {
    list_.setItems(std::move(items));
    const int nav = sideNavWidth(v_->text_);
    placeCells(v_->listView_.obj(), 0, 1, v_->text_.cols() - nav, bottom);
    v_->listView_.sync(list_);
    v_->navView_.sync(nav, 1, bottom, v_->text_.cols());
    if (ids_.empty()) v_->text_.line(1, 3, "no conversations yet", t.dim);
  }
  v_->text_.end();
  lvui::setHidden(v_->inputView_.obj(), !editing_);
  if (editing_) {
    input_.setDisabled(busy_);
    placeCells(v_->inputView_.obj(), 0, bottom, v_->text_.cols(), bottom + 1);
    v_->inputView_.sync(input_, U"to> @");
    syncHint(v_->hint_, busy_ ? U"starting\u2026  back: cancel" : U"type a username  \u25CF start  back: cancel",
             busy_ ? U"starting\u2026" : U"type a username  enter: start");
  } else if (buttons_) {
    syncHint(v_->hint_, U"", U"tap to open  drag to scroll");
  } else {
    syncHint(v_->hint_, U"\u2191\u2193 pick  \u25CF open  \u2190 home", U"tap to open  swipe to scroll");
  }
}

}  // namespace rc

#endif  // RC_FEAT_CMAIL
