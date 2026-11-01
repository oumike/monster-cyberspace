#include "features.h"
#if RC_FEAT_PEOPLE

#include "screens/people_nav.h"

#include <vector>

#include "screens/router.h"

namespace rc::people {

namespace {

constexpr size_t kMaxTrail = 8;

struct Origin {
  router::Place here, base;
};

std::vector<Origin> trail;

}  // namespace

void openProfile(const std::string& username) {
  if (username.empty()) return;
  if (trail.size() >= kMaxTrail) trail.erase(trail.begin());
  trail.push_back(Origin{router::here(), router::lastBase()});
  router::go(router::Id::Profile, username);
}

void back() {
  if (trail.empty()) return router::go(router::Id::Home);
  Origin o = trail.back();
  trail.pop_back();
  // An Entry or the Composer goes back to the list it came from, which
  // opening the profile replaced as lastBase().
  router::setBase(o.base);
  router::go(o.here.id, o.here.arg);
}

}  // namespace rc::people

#endif  // RC_FEAT_PEOPLE
