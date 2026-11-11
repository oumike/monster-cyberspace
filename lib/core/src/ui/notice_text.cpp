// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/notice_text.h"

#include "feed/markdown_text.h"
#include "ui/guild_layout.h"

namespace rc {

namespace {

#define RC_MENTION_TYPES "chat_mention,reply,thread_reply,post_mention,reply_mention,post_cooldown,guild_new_thread"
#define RC_SOCIAL_TYPES "new_follower,poke,bookmark,new_post_following,new_post_friend"

bool endsWith(std::string_view s, std::string_view tail) {
  return s.size() >= tail.size() && s.substr(s.size() - tail.size()) == tail;
}

std::string cut(std::string s, size_t n) {
  truncateCodePoints(s, n);
  return s;
}

}  // namespace

bool isSocialNotice(std::string_view t) {
  return t == "new_follower" || t == "poke" || t == "bookmark" || t == "new_post_following" ||
         t == "new_post_friend";
}

NoticeView describeNotice(const Mention& m, size_t n) {
  NoticeView v;
  const std::string at = "@" + m.actorUsername;
  if (m.type == "new_follower" || m.type == "poke") {
    if (m.actorUsername.empty() || m.actorUsername == "system") return v;
    v.open = NoticeOpen::Profile;
    v.user = m.actorUsername;
    v.text = at + (m.type == "poke" ? " poked you" : " followed you");
    return v;
  }
  if (m.targetId.empty()) return v;
  if (m.type == "bookmark") {
    v.text = at + " bookmarked your " + (m.targetType == "reply" ? "reply" : "entry");
  } else if (m.type == "new_post_following" || m.type == "new_post_friend") {
    const std::string& what = m.postTitle.empty() ? m.preview : m.postTitle;
    v.text = what.empty() ? at + " posted an entry" : at + " posted: " + cut(what, n);
  } else if (m.type == "guild_new_thread") {
    v.text = guildNoticeText(m.actorUsername, m.guildName, cut(m.preview, n));
  } else if (m.type == "reply" || m.type == "thread_reply" || endsWith(m.type, "_mention")) {
    if (m.type == "chat_mention") return v;
    v.text = at + (endsWith(m.type, "_mention") ? " mentioned you: " : " replied: ") + cut(m.preview, n);
  } else {
    return v;
  }
  v.open = NoticeOpen::Entry;
  return v;
}

const char* noticeTypes(UnreadFilter f) {
  return f == UnreadFilter::Mentions ? RC_MENTION_TYPES : RC_MENTION_TYPES "," RC_SOCIAL_TYPES;
}

UnreadFilter nextUnreadFilter(UnreadFilter f) {
  return f == UnreadFilter::All ? UnreadFilter::Mentions : UnreadFilter::All;
}

UnreadFilter unreadFilterFromIndex(int i) {
  return i == static_cast<int>(UnreadFilter::Mentions) ? UnreadFilter::Mentions : UnreadFilter::All;
}

const char* unreadFilterKey(UnreadFilter f) { return f == UnreadFilter::Mentions ? "mentions" : "all"; }

bool parseUnreadFilter(std::string_view key, UnreadFilter& out) {
  for (UnreadFilter f : {UnreadFilter::All, UnreadFilter::Mentions}) {
    if (key == unreadFilterKey(f)) {
      out = f;
      return true;
    }
  }
  return false;
}

std::u32string unreadFilterLabel(UnreadFilter f) {
  return f == UnreadFilter::Mentions ? U"Replies & mentions" : U"All";
}

}  // namespace rc
