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
