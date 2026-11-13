#include "app/queues.h"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>

namespace rc {

namespace {

std::mutex cmdMu, evtMu;
std::condition_variable cmdCv;
std::deque<Command> cmds;
std::deque<Event> evts;

}  // namespace

bool postCommand(Command c) {
  {
    std::lock_guard<std::mutex> g(cmdMu);
    if (cmds.size() >= kQueueDepth) return false;
    cmds.push_back(std::move(c));
  }
  cmdCv.notify_one();
  return true;
}

void refuseCommand(const Command& c) {
  const char* busy = "Busy, try again";
  switch (c.type) {
    case CmdType::Send:
      postEvent(Event{EvtType::SendDone, busy, 0, false});
      return;
    case CmdType::PostEntry:
      postEvent(Event{EvtType::EntryPosted, busy, 0, false});
      return;
    case CmdType::PostReply:
    case CmdType::EditReply:
      postEvent(Event{EvtType::FeedWriteDone, busy, 0, false});
      return;
    case CmdType::EditEntry:
      postEvent(Event{EvtType::EntryEdited, busy, 0, false});
      return;
    case CmdType::StartConversation:
      postEvent(Event{EvtType::ConversationStarted, busy, 0, false});
      return;
    case CmdType::MarkAllRead:
      postEvent(Event{EvtType::AllRead, busy, 0, false});
      return;
    case CmdType::MarkRoomsRead:
      postEvent(Event{EvtType::RoomsRead, busy, 0, false});
      return;
    case CmdType::OpenEntry:
      postEvent(Event{EvtType::EntryUpdated, "", 0, false});
      break;
    case CmdType::LoadOlder:
      postEvent(Event{EvtType::OlderLoaded, "", 0, true});
      break;
    case CmdType::MoreReplies:
      postEvent(Event{EvtType::RepliesLoaded});
      break;
    case CmdType::OpenFeed:
    case CmdType::FeedMore:
    case CmdType::FeedRefresh:
      postEvent(Event{EvtType::FeedUpdated});
      break;
    case CmdType::OpenWatched:
    case CmdType::OpenBookmarks:
    case CmdType::OpenTopic:
    case CmdType::OpenGuild:
    case CmdType::GuildThreadsMore:
    case CmdType::OpenUserPosts:
    case CmdType::OpenFollowingPosts:
      postEvent(Event{EvtType::SideListUpdated, "", 0, false});
      break;
    case CmdType::OpenGuilds:
      postEvent(Event{EvtType::GuildsUpdated, "", 0, false});
      break;
    case CmdType::OpenTopics:
      postEvent(Event{EvtType::TopicsUpdated, "", 0, false});
      break;
    case CmdType::OpenMembers:
      postEvent(Event{EvtType::MembersUpdated, "", 0, false});
      break;
    case CmdType::PostGuildThread:
      postEvent(Event{EvtType::EntryPosted, busy, 0, false});
      return;
    case CmdType::JoinGuild:
    case CmdType::LeaveGuild:
    case CmdType::PromoteGuild:
      postEvent(Event{EvtType::GuildWriteDone, busy, 0, false});
      return;
    case CmdType::OpenProfile:
      postEvent(Event{EvtType::ProfileUpdated, "", 0, false});
      break;
    case CmdType::Follow:
    case CmdType::Unfollow:
    case CmdType::Poke:
      postEvent(Event{EvtType::PeopleWriteDone, busy, 0, false});
      return;
    case CmdType::SaveProfile:
      postEvent(Event{EvtType::ProfileSaved, busy, 0, false});
      return;
    case CmdType::OpenUserReplies:
      postEvent(Event{EvtType::UserRepliesUpdated, "", 0, false});
      break;
    case CmdType::OpenUserGuilds:
      postEvent(Event{EvtType::UserGuildsUpdated, "", 0, false});
      break;
    case CmdType::OpenNotes:
      postEvent(Event{EvtType::NotesUpdated, "", 0, false});
      break;
    case CmdType::OpenNote:
      postEvent(Event{EvtType::NoteUpdated, "", 0, false});
      break;
    case CmdType::OpenNoteHistory:
      postEvent(Event{EvtType::NoteHistoryUpdated, "", 0, false});
      break;
    case CmdType::SaveNote:
      postEvent(Event{EvtType::NoteSaved, busy, 0, false});
      return;
    case CmdType::OpenPrograms:
    case CmdType::MorePrograms:
      postEvent(Event{EvtType::ProgramsLoaded, "", 0, false});
      break;
    case CmdType::OpenProgram:
      postEvent(Event{EvtType::ProgramLoaded, c.a + "\n" + std::to_string(c.n), 0, false});
      break;
    case CmdType::Search:
      postEvent(Event{EvtType::SearchUpdated, "all", 0, false});
      break;
    case CmdType::SearchMore:
      postEvent(Event{EvtType::SearchUpdated, c.a, 0, false});
      break;
    case CmdType::DeleteNote:
      postEvent(Event{EvtType::NoteDeleted, busy, 0, false});
      return;
    default:
      break;
  }
  postEvent(Event{EvtType::Banner, busy, 0, false});
}

bool nextCommand(Command& c, uint32_t waitMs) {
  std::unique_lock<std::mutex> g(cmdMu);
  if (!cmdCv.wait_for(g, std::chrono::milliseconds(waitMs), [] { return !cmds.empty(); }))
    return false;
  c = std::move(cmds.front());
  cmds.pop_front();
  return true;
}

bool postEvent(Event e) {
  std::lock_guard<std::mutex> g(evtMu);
  if (evts.size() >= kQueueDepth) {
    auto it = std::find_if(evts.begin(), evts.end(),
                           [](const Event& x) { return x.type == EvtType::RoomChanged; });
    evts.erase(it != evts.end() ? it : evts.begin());
  }
  evts.push_back(std::move(e));
  return true;
}

bool nextEvent(Event& e) {
  std::lock_guard<std::mutex> g(evtMu);
  if (evts.empty()) return false;
  e = std::move(evts.front());
  evts.pop_front();
  return true;
}

}  // namespace rc
