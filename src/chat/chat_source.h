#pragma once

#include "app/messages.h"
#include "store/settings.h"

namespace rc {

// Where screens send commands. The network build posts them to the net
// worker; the m9_fake build applies them to scripted fixtures.
class ChatSource {
 public:
  virtual ~ChatSource() = default;
  virtual void command(const Command&) = 0;
};

// Defined by whichever implementation is linked.
ChatSource& chatSource();
// Starts the source's task. The network source also owns Wi-Fi from here on.
void startChatSource(const Settings&);

}  // namespace rc
