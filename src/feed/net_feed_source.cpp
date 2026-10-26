#if !RC_FAKE_API

#include "app/queues.h"
#include "feed/feed_source.h"

namespace rc {

namespace {

class NetFeedSource : public FeedSource {
 public:
  void command(const Command& c) override {
    if (!postCommand(c)) refuseCommand(c);
  }
};

}  // namespace

FeedSource& feedSource() {
  static NetFeedSource s;
  return s;
}

}  // namespace rc

#endif
