#if !RC_FAKE_API

#include "app/net_worker.h"
#include "app/queues.h"
#include "chat/chat_source.h"

namespace rc {

namespace {

class NetChatSource : public ChatSource {
 public:
  void command(const Command& c) override {
    if (!postCommand(c)) postEvent(Event{EvtType::Banner, "Busy, try again", 0, false});
  }
};

}  // namespace

ChatSource& chatSource() {
  static NetChatSource s;
  return s;
}

void startChatSource(const Settings& s) { networker::start(s); }

}  // namespace rc

#endif
