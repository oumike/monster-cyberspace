#pragma once

#include "app/messages.h"

namespace rc {

// Where Feed screens send commands. The network build posts them to the net
// worker; the demo build (RC_FAKE_API) applies them to scripted fixtures.
class FeedSource {
 public:
  virtual ~FeedSource() = default;
  virtual void command(const Command&) = 0;
};

// Defined by whichever implementation is linked.
FeedSource& feedSource();

}  // namespace rc
